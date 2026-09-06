#ifndef SYNC_MANAGER_HPP
#define SYNC_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include "PhotoScanner.hpp"
#include "ImmichClient.hpp"
#include "Config.hpp"

enum class SyncState {
    IDLE,
    SCANNING,
    CALCULATING_HASH,
    CHECKING_SERVER,
    UPLOADING,
    COMPLETED,
    FAILED
};

struct SyncRecord {
    std::string path;
    std::string filename;
    size_t fileSize = 0;
    time_t modTime = 0;
    std::string sha1;
    std::string assetId;
    std::string syncedAt;
    MediaType mediaType = MediaType::PHOTO;
};

class SyncManager {
public:
    SyncManager(ImmichClient& client, AppConfig& config, const std::string& syncDbPath = "sdmc:/3ds/Immich3DS/sync.json");
    ~SyncManager();

    bool loadSyncDb();
    bool saveSyncDb();

    void scanLocalPhotos();
    bool performSync();

    // State & Statistics
    SyncState getState() const { return m_state; }
    size_t getTotalMediaCount() const { return m_allPhotos.size(); }
    size_t getTotalPhotosCount() const;
    size_t getTotalVideosCount() const;
    size_t getUnsyncedMediaCount() const { return m_unsyncedPhotos.size(); }
    size_t getUnsyncedPhotosCount() const;
    size_t getUnsyncedVideosCount() const;
    size_t getCurrentSyncIndex() const { return m_currentSyncIndex; }
    size_t getTotalSyncCount() const { return m_totalToSync; }
    float getCurrentFileProgress() const { return m_currentFileProgress; }
    size_t getCurrentFileNow() const { return m_currentFileNow; }
    size_t getCurrentFileTotal() const { return m_currentFileTotal; }
    std::string getCurrentFilename() const { return m_currentFilename; }
    MediaType getCurrentMediaType() const { return m_currentMediaType; }
    std::string getLastError() const { return m_lastError; }
    const std::vector<PhotoInfo>& getUnsyncedPhotos() const { return m_unsyncedPhotos; }

    void cancelSync() { m_cancelRequested = true; }

private:
    ImmichClient& m_client;
    AppConfig& m_config;
    std::string m_syncDbPath;

    std::map<std::string, SyncRecord> m_syncRecords; // Key: filename or SHA-1
    std::vector<PhotoInfo> m_allPhotos;
    std::vector<PhotoInfo> m_unsyncedPhotos;

    SyncState m_state = SyncState::IDLE;
    size_t m_currentSyncIndex = 0;
    size_t m_totalToSync = 0;
    float m_currentFileProgress = 0.0f;
    size_t m_currentFileNow = 0;
    size_t m_currentFileTotal = 0;
    std::string m_currentFilename;
    MediaType m_currentMediaType = MediaType::PHOTO;
    std::string m_lastError;
    bool m_cancelRequested = false;

    static void uploadProgressCallback(size_t now, size_t total, void* user);
};

#endif // SYNC_MANAGER_HPP
