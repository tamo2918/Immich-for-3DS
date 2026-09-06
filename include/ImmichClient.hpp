#ifndef IMMICH_CLIENT_HPP
#define IMMICH_CLIENT_HPP

#include <string>
#include <vector>
#include "PhotoScanner.hpp"

struct ServerVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string toString() const;
};

struct UserInfo {
    std::string id;
    std::string email;
    std::string name;
};

struct BulkCheckResult {
    std::string id; // Local filename/id
    std::string action; // "accept" or "reject"
    std::string reason; // "duplicate" etc.
    std::string assetId;
    bool isTrashed = false;
};

struct RemoteAsset {
    std::string id;
    std::string filename;
    std::string createdAt;
    std::string type;
};

enum class ImmichError {
    SUCCESS,
    NETWORK_ERROR,
    HTTP_ERROR,
    UNAUTHORIZED,
    FORBIDDEN,
    SERVER_ERROR,
    INVALID_JSON,
    FILE_NOT_FOUND,
    OUT_OF_MEMORY
};

class ImmichClient {
public:
    ImmichClient(const std::string& serverUrl, const std::string& apiKey, bool sslVerify = true, int timeoutSec = 15);
    ~ImmichClient();

    static void globalInit();
    static void globalCleanup();

    void updateConfig(const std::string& serverUrl, const std::string& apiKey, bool sslVerify, int timeoutSec);

    // Health & Auth
    ImmichError ping(std::string& outMessage);
    ImmichError getVersion(ServerVersion& outVersion);
    ImmichError getMe(UserInfo& outUser);

    // Deduplication & Upload
    ImmichError checkBulkUpload(const std::vector<PhotoInfo>& photos, std::vector<BulkCheckResult>& outResults);
    ImmichError uploadAsset(const PhotoInfo& photo, std::string& outAssetId, bool& outDuplicate,
                            void (*progressCallback)(size_t now, size_t total, void* user) = nullptr,
                            void* callbackUser = nullptr);

    // Gallery
    ImmichError searchAssets(int page, int size, std::vector<RemoteAsset>& outAssets, int& outTotal);
    ImmichError getAssetThumbnail(const std::string& assetId, const std::string& size, std::vector<uint8_t>& outBytes);

    std::string getLastError() const { return m_lastError; }
    long getLastHttpCode() const { return m_lastHttpCode; }

private:
    std::string m_serverUrl;
    std::string m_apiKey;
    bool m_sslVerify;
    int m_timeoutSec;
    std::string m_lastError;
    long m_lastHttpCode = 0;

    void setupCommonCurl(void* curlHandle, const std::string& url, void* headerList);
};

#endif // IMMICH_CLIENT_HPP
