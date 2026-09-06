#include "SyncManager.hpp"
#include "cJSON.h"
#include "Logger.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

SyncManager::SyncManager(ImmichClient& client, AppConfig& config, const std::string& syncDbPath)
    : m_client(client), m_config(config), m_syncDbPath(syncDbPath) {
}

SyncManager::~SyncManager() {
}

bool SyncManager::loadSyncDb() {
    m_syncRecords.clear();

    FILE* f = fopen(m_syncDbPath.c_str(), "rb");
    if (!f) {
        Logger::info("No sync database found at %s. Starting fresh.", m_syncDbPath.c_str());
        return true;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return true;
    }

    char* buffer = (char*)malloc(sz + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }

    fread(buffer, 1, sz, f);
    buffer[sz] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
        Logger::warn("Failed to parse sync database JSON at %s", m_syncDbPath.c_str());
        return false;
    }

    cJSON* recordsArray = cJSON_GetObjectItem(root, "records");
    if (recordsArray && cJSON_IsArray(recordsArray)) {
        cJSON* item;
        cJSON_ArrayForEach(item, recordsArray) {
            SyncRecord rec;
            cJSON* val = cJSON_GetObjectItem(item, "path");
            if (val && cJSON_IsString(val) && val->valuestring) rec.path = val->valuestring;

            val = cJSON_GetObjectItem(item, "filename");
            if (val && cJSON_IsString(val) && val->valuestring) rec.filename = val->valuestring;

            val = cJSON_GetObjectItem(item, "size");
            if (val && cJSON_IsNumber(val)) rec.fileSize = (size_t)val->valuedouble;

            val = cJSON_GetObjectItem(item, "modTime");
            if (val && cJSON_IsNumber(val)) rec.modTime = (time_t)val->valuedouble;

            val = cJSON_GetObjectItem(item, "sha1");
            if (val && cJSON_IsString(val) && val->valuestring) rec.sha1 = val->valuestring;

            val = cJSON_GetObjectItem(item, "assetId");
            if (val && cJSON_IsString(val) && val->valuestring) rec.assetId = val->valuestring;

            val = cJSON_GetObjectItem(item, "syncedAt");
            if (val && cJSON_IsString(val) && val->valuestring) rec.syncedAt = val->valuestring;

            if (!rec.filename.empty()) {
                m_syncRecords[rec.filename] = rec;
            }
        }
    }

    cJSON_Delete(root);
    Logger::info("Loaded %zu record(s) from sync database.", m_syncRecords.size());
    return true;
}

bool SyncManager::saveSyncDb() {
    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "records", arr);

    for (const auto& pair : m_syncRecords) {
        const SyncRecord& rec = pair.second;
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "path", rec.path.c_str());
        cJSON_AddStringToObject(item, "filename", rec.filename.c_str());
        cJSON_AddNumberToObject(item, "size", (double)rec.fileSize);
        cJSON_AddNumberToObject(item, "modTime", (double)rec.modTime);
        cJSON_AddStringToObject(item, "sha1", rec.sha1.c_str());
        cJSON_AddStringToObject(item, "assetId", rec.assetId.c_str());
        cJSON_AddStringToObject(item, "syncedAt", rec.syncedAt.c_str());
        cJSON_AddItemToArray(arr, item);
    }

    char* jsonStr = cJSON_Print(root);
    cJSON_Delete(root);

    if (!jsonStr) return false;

    std::string tmpPath = m_syncDbPath + ".tmp";
    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (!f) {
        cJSON_free(jsonStr);
        Logger::error("Failed to open %s for writing sync database.", tmpPath.c_str());
        return false;
    }

    fputs(jsonStr, f);
    fclose(f);
    cJSON_free(jsonStr);

    remove(m_syncDbPath.c_str());
    if (rename(tmpPath.c_str(), m_syncDbPath.c_str()) != 0) {
        Logger::error("Failed to rename temp sync database to %s", m_syncDbPath.c_str());
        return false;
    }

    return true;
}

void SyncManager::scanLocalPhotos() {
    m_state = SyncState::SCANNING;
    m_allPhotos = PhotoScanner::scanDcim("sdmc:/DCIM", m_config.syncMpo);
    m_unsyncedPhotos.clear();

    for (const auto& photo : m_allPhotos) {
        auto it = m_syncRecords.find(photo.filename);
        if (it == m_syncRecords.end()) {
            // Not in sync database
            m_unsyncedPhotos.push_back(photo);
        } else {
            // Check if file size or modTime changed
            if (it->second.fileSize != photo.fileSize || it->second.modTime != photo.modTime) {
                m_unsyncedPhotos.push_back(photo);
            }
        }
    }

    m_state = SyncState::IDLE;
    Logger::info("Scan complete. Total: %zu, Unsynced: %zu", m_allPhotos.size(), m_unsyncedPhotos.size());
}

void SyncManager::uploadProgressCallback(size_t now, size_t total, void* user) {
    SyncManager* mgr = static_cast<SyncManager*>(user);
    if (mgr) {
        mgr->m_currentFileNow = now;
        if (total > 0) {
            mgr->m_currentFileTotal = total;
            mgr->m_currentFileProgress = (float)now / (float)total;
        } else if (mgr->m_currentFileTotal > 0) {
            mgr->m_currentFileProgress = (float)now / (float)mgr->m_currentFileTotal;
        }
    }
}

bool SyncManager::performSync() {
    m_cancelRequested = false;
    m_lastError.clear();

    scanLocalPhotos();
    if (m_unsyncedPhotos.empty()) {
        Logger::info("All photos are already synchronized.");
        m_state = SyncState::COMPLETED;
        return true;
    }

    // Phase 1: Calculate SHA-1 hashes
    m_state = SyncState::CALCULATING_HASH;
    for (size_t i = 0; i < m_unsyncedPhotos.size(); i++) {
        if (m_cancelRequested) {
            m_state = SyncState::IDLE;
            return false;
        }
        m_currentFilename = m_unsyncedPhotos[i].filename;
        m_currentSyncIndex = i + 1;
        m_totalToSync = m_unsyncedPhotos.size();
        PhotoScanner::calculateSha1(m_unsyncedPhotos[i]);
    }

    // Phase 2: Server Bulk Upload Check
    m_state = SyncState::CHECKING_SERVER;
    std::vector<BulkCheckResult> checkResults;
    ImmichError chkErr = m_client.checkBulkUpload(m_unsyncedPhotos, checkResults);
    if (chkErr == ImmichError::UNAUTHORIZED || chkErr == ImmichError::FORBIDDEN) {
        m_lastError = m_client.getLastError();
        m_state = SyncState::FAILED;
        return false;
    }

    // Mark photos that server already has as synced
    std::map<std::string, BulkCheckResult> resultMap;
    for (const auto& res : checkResults) {
        resultMap[res.id] = res;
    }

    std::vector<PhotoInfo> actuallyNeedUpload;
    for (const auto& photo : m_unsyncedPhotos) {
        auto it = resultMap.find(photo.filename);
        if (it != resultMap.end() && it->second.action == "reject" && it->second.reason == "duplicate") {
            // Already on server! Record in sync database
            SyncRecord rec;
            rec.path = photo.path;
            rec.filename = photo.filename;
            rec.fileSize = photo.fileSize;
            rec.modTime = photo.modTime;
            rec.sha1 = photo.sha1;
            rec.assetId = it->second.assetId;

            time_t now = time(nullptr);
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            rec.syncedAt = tbuf;

            m_syncRecords[rec.filename] = rec;
            Logger::info("Skipped duplicate on server: %s (Asset ID: %s)",
                         photo.filename.c_str(), rec.assetId.c_str());
        } else {
            actuallyNeedUpload.push_back(photo);
        }
    }

    // Save database after bulk check
    saveSyncDb();

    if (actuallyNeedUpload.empty()) {
        m_state = SyncState::COMPLETED;
        time_t now = time(nullptr);
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y/%m/%d %H:%M", localtime(&now));
        m_config.lastSyncTime = tbuf;
        ConfigManager::save("sdmc:/3ds/Immich3DS/config.json", m_config);
        scanLocalPhotos();
        return true;
    }

    // Phase 3: Upload each file
    m_state = SyncState::UPLOADING;
    m_totalToSync = actuallyNeedUpload.size();

    for (size_t i = 0; i < actuallyNeedUpload.size(); i++) {
        if (m_cancelRequested) {
            m_state = SyncState::IDLE;
            return false;
        }

        const PhotoInfo& photo = actuallyNeedUpload[i];
        m_currentSyncIndex = i + 1;
        m_currentFilename = photo.filename;
        m_currentFileProgress = 0.0f;
        m_currentFileNow = 0;
        m_currentFileTotal = photo.fileSize;

        std::string assetId;
        bool duplicate = false;
        ImmichError upErr = m_client.uploadAsset(photo, assetId, duplicate, uploadProgressCallback, this);

        if (upErr != ImmichError::SUCCESS) {
            m_lastError = m_client.getLastError();
            Logger::error("Failed to upload %s: %s", photo.filename.c_str(), m_lastError.c_str());
            m_state = SyncState::FAILED;
            saveSyncDb();
            return false;
        }

        // Successfully uploaded!
        SyncRecord rec;
        rec.path = photo.path;
        rec.filename = photo.filename;
        rec.fileSize = photo.fileSize;
        rec.modTime = photo.modTime;
        rec.sha1 = photo.sha1;
        rec.assetId = assetId;

        time_t now = time(nullptr);
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        rec.syncedAt = tbuf;

        m_syncRecords[rec.filename] = rec;
        saveSyncDb(); // Immediate persistence
    }

    // Update last sync time
    time_t now = time(nullptr);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y/%m/%d %H:%M", localtime(&now));
    m_config.lastSyncTime = tbuf;
    ConfigManager::save("sdmc:/3ds/Immich3DS/config.json", m_config);

    m_currentFilename.clear();
    m_currentFileProgress = 0.0f;
    m_currentFileNow = 0;
    m_currentFileTotal = 0;
    m_state = SyncState::COMPLETED;
    Logger::info("Sync completed successfully! %zu photos uploaded.", actuallyNeedUpload.size());
    scanLocalPhotos();
    return true;
}
