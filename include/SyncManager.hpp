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
    size_t getTotalPhotosCount() const { return m_allPhotos.size(); }
    size_t getUnsyncedPhotosCount() const { return m_unsyncedPhotos.size(); }
    size_t getCurrentSyncIndex() const { return m_currentSyncIndex; }
    size_t getTotalSyncCount() const { return m_totalToSync; }
    float getCurrentFileProgress() const { return m_currentFileProgress; }
    std::string getCurrentFilename() const { return m_currentFilename; }
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
    std::string m_currentFilename;
    std::string m_lastError;
    bool m_cancelRequested = false;

    static void uploadProgressCallback(size_t now, size_t total, void* user);
};

#endif // SYNC_MANAGER_HPP
