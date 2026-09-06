#include "ImmichClient.hpp"
#include "cJSON.h"
#include "Logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <cstring>
#include <cstdio>

static size_t stringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

static size_t binaryWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::vector<uint8_t>* vec = static_cast<std::vector<uint8_t>*>(userp);
    const uint8_t* bytes = static_cast<const uint8_t*>(contents);
    vec->insert(vec->end(), bytes, bytes + total);
    return total;
}

struct ProgressContext {
    void (*callback)(size_t now, size_t total, void* user);
    void* user;
};

static int xferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;
    (void)dlnow;
    ProgressContext* ctx = static_cast<ProgressContext*>(clientp);
    if (ctx && ctx->callback && ultotal > 0) {
        ctx->callback((size_t)ulnow, (size_t)ultotal, ctx->user);
    }
    return 0;
}

std::string ServerVersion::toString() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "v%d.%d.%d", major, minor, patch);
    return std::string(buf);
}

void ImmichClient::globalInit() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void ImmichClient::globalCleanup() {
    curl_global_cleanup();
}

ImmichClient::ImmichClient(const std::string& serverUrl, const std::string& apiKey, bool sslVerify, int timeoutSec)
    : m_serverUrl(serverUrl), m_apiKey(apiKey), m_sslVerify(sslVerify), m_timeoutSec(timeoutSec) {
}

ImmichClient::~ImmichClient() {
}

void ImmichClient::updateConfig(const std::string& serverUrl, const std::string& apiKey, bool sslVerify, int timeoutSec) {
    m_serverUrl = serverUrl;
    m_apiKey = apiKey;
    m_sslVerify = sslVerify;
    m_timeoutSec = timeoutSec;
}

void ImmichClient::setupCommonCurl(void* curlHandle, const std::string& url, void* headerList) {
    CURL* curl = static_cast<CURL*>(curlHandle);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)m_timeoutSec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Immich3DS/1.0 (Nintendo 3DS)");

    if (m_sslVerify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        // Check if romfs CA bundle exists
        FILE* ca = fopen("romfs:/cacert.pem", "r");
        if (ca) {
            fclose(ca);
            curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<struct curl_slist*>(headerList));
    }
}

ImmichError ImmichClient::ping(std::string& outMessage) {
    m_lastError.clear();
    m_lastHttpCode = 0;

    CURL* curl = curl_easy_init();
    if (!curl) {
        m_lastError = "curl_easy_init failed";
        return ImmichError::NETWORK_ERROR;
    }

    std::string url = m_serverUrl + "/api/server/ping";
    std::string response;
    setupCommonCurl(curl, url, nullptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        Logger::error("Ping failed: %s (%s)", m_lastError.c_str(), url.c_str());
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 200) {
        outMessage = "OK";
        Logger::info("Ping successful: %s -> %s", url.c_str(), response.c_str());
        return ImmichError::SUCCESS;
    }

    m_lastError = "HTTP status " + std::to_string(m_lastHttpCode);
    return ImmichError::HTTP_ERROR;
}

ImmichError ImmichClient::getVersion(ServerVersion& outVersion) {
    m_lastError.clear();
    m_lastHttpCode = 0;

    CURL* curl = curl_easy_init();
    if (!curl) return ImmichError::NETWORK_ERROR;

    std::string url = m_serverUrl + "/api/server/version";
    std::string response;
    setupCommonCurl(curl, url, nullptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode != 200) {
        m_lastError = "HTTP " + std::to_string(m_lastHttpCode);
        return ImmichError::HTTP_ERROR;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        m_lastError = "Invalid JSON in version response";
        return ImmichError::INVALID_JSON;
    }

    cJSON* item = cJSON_GetObjectItem(root, "major");
    if (item && cJSON_IsNumber(item)) outVersion.major = item->valueint;

    item = cJSON_GetObjectItem(root, "minor");
    if (item && cJSON_IsNumber(item)) outVersion.minor = item->valueint;

    item = cJSON_GetObjectItem(root, "patch");
    if (item && cJSON_IsNumber(item)) outVersion.patch = item->valueint;

    cJSON_Delete(root);
    Logger::info("Immich Server Version: %s", outVersion.toString().c_str());
    return ImmichError::SUCCESS;
}

ImmichError ImmichClient::getMe(UserInfo& outUser) {
    m_lastError.clear();
    m_lastHttpCode = 0;

    CURL* curl = curl_easy_init();
    if (!curl) return ImmichError::NETWORK_ERROR;

    std::string url = m_serverUrl + "/api/users/me";
    struct curl_slist* headers = nullptr;
    std::string keyHeader = "x-api-key: " + m_apiKey;
    headers = curl_slist_append(headers, keyHeader.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string response;
    setupCommonCurl(curl, url, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 401) {
        m_lastError = "Unauthorized: Invalid API Key";
        return ImmichError::UNAUTHORIZED;
    }
    if (m_lastHttpCode == 403) {
        m_lastError = "Forbidden: API key lacks 'user.read' scope";
        return ImmichError::FORBIDDEN;
    }
    if (m_lastHttpCode != 200) {
        m_lastError = "HTTP " + std::to_string(m_lastHttpCode);
        return ImmichError::HTTP_ERROR;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        m_lastError = "Invalid JSON in /users/me response";
        return ImmichError::INVALID_JSON;
    }

    cJSON* item = cJSON_GetObjectItem(root, "id");
    if (item && cJSON_IsString(item) && item->valuestring) outUser.id = item->valuestring;

    item = cJSON_GetObjectItem(root, "email");
    if (item && cJSON_IsString(item) && item->valuestring) outUser.email = item->valuestring;

    item = cJSON_GetObjectItem(root, "name");
    if (item && cJSON_IsString(item) && item->valuestring) outUser.name = item->valuestring;

    cJSON_Delete(root);
    Logger::info("User info: %s <%s>", outUser.name.c_str(), outUser.email.c_str());
    return ImmichError::SUCCESS;
}

ImmichError ImmichClient::checkBulkUpload(const std::vector<PhotoInfo>& photos, std::vector<BulkCheckResult>& outResults) {
    m_lastError.clear();
    m_lastHttpCode = 0;
    outResults.clear();

    if (photos.empty()) return ImmichError::SUCCESS;

    cJSON* root = cJSON_CreateObject();
    cJSON* assetsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "assets", assetsArray);

    for (const auto& photo : photos) {
        if (photo.sha1.empty()) continue;
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", photo.filename.c_str());
        cJSON_AddStringToObject(item, "checksum", photo.sha1.c_str());
        cJSON_AddItemToArray(assetsArray, item);
    }

    char* postBody = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!postBody) {
        m_lastError = "Failed to construct bulk-upload-check JSON";
        return ImmichError::OUT_OF_MEMORY;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        cJSON_free(postBody);
        return ImmichError::NETWORK_ERROR;
    }

    std::string url = m_serverUrl + "/api/assets/bulk-upload-check";
    struct curl_slist* headers = nullptr;
    std::string keyHeader = "x-api-key: " + m_apiKey;
    headers = curl_slist_append(headers, keyHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string response;
    setupCommonCurl(curl, url, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    cJSON_free(postBody);

    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 401) return ImmichError::UNAUTHORIZED;
    if (m_lastHttpCode == 403) return ImmichError::FORBIDDEN;
    if (m_lastHttpCode != 200) {
        m_lastError = "HTTP " + std::to_string(m_lastHttpCode);
        return ImmichError::HTTP_ERROR;
    }

    cJSON* respJson = cJSON_Parse(response.c_str());
    if (!respJson) {
        m_lastError = "Failed to parse bulk-upload-check response";
        return ImmichError::INVALID_JSON;
    }

    cJSON* resultsArray = cJSON_GetObjectItem(respJson, "results");
    if (resultsArray && cJSON_IsArray(resultsArray)) {
        cJSON* resItem;
        cJSON_ArrayForEach(resItem, resultsArray) {
            BulkCheckResult r;
            cJSON* val = cJSON_GetObjectItem(resItem, "id");
            if (val && cJSON_IsString(val) && val->valuestring) r.id = val->valuestring;

            val = cJSON_GetObjectItem(resItem, "action");
            if (val && cJSON_IsString(val) && val->valuestring) r.action = val->valuestring;

            val = cJSON_GetObjectItem(resItem, "reason");
            if (val && cJSON_IsString(val) && val->valuestring) r.reason = val->valuestring;

            val = cJSON_GetObjectItem(resItem, "assetId");
            if (val && cJSON_IsString(val) && val->valuestring) r.assetId = val->valuestring;

            val = cJSON_GetObjectItem(resItem, "isTrashed");
            if (val && cJSON_IsBool(val)) r.isTrashed = cJSON_IsTrue(val);

            outResults.push_back(r);
        }
    }

    cJSON_Delete(respJson);
    Logger::info("Bulk upload check returned %zu result(s)", outResults.size());
    return ImmichError::SUCCESS;
}

ImmichError ImmichClient::uploadAsset(const PhotoInfo& photo, std::string& outAssetId, bool& outDuplicate,
                                     void (*progressCallback)(size_t now, size_t total, void* user),
                                     void* callbackUser) {
    m_lastError.clear();
    m_lastHttpCode = 0;
    outAssetId.clear();
    outDuplicate = false;

    // Verify file exists
    FILE* testFp = fopen(photo.path.c_str(), "rb");
    if (!testFp) {
        m_lastError = "Cannot open local file: " + photo.path;
        Logger::error("%s", m_lastError.c_str());
        return ImmichError::FILE_NOT_FOUND;
    }
    fclose(testFp);

    CURL* curl = curl_easy_init();
    if (!curl) return ImmichError::NETWORK_ERROR;

    curl_mime* mime = curl_mime_init(curl);
    if (!mime) {
        curl_easy_cleanup(curl);
        return ImmichError::OUT_OF_MEMORY;
    }

    // 1. assetData part: streams directly from SD card path!
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "assetData");
    curl_mime_filedata(part, photo.path.c_str());
    curl_mime_filename(part, photo.filename.c_str());

    // 2. fileCreatedAt & fileModifiedAt
    std::string isoTime = photo.getIsoTime();
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "fileCreatedAt");
    curl_mime_data(part, isoTime.c_str(), CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "fileModifiedAt");
    curl_mime_data(part, isoTime.c_str(), CURL_ZERO_TERMINATED);

    // 3. isFavorite
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "isFavorite");
    curl_mime_data(part, "false", CURL_ZERO_TERMINATED);

    std::string url = m_serverUrl + "/api/assets";
    struct curl_slist* headers = nullptr;
    std::string keyHeader = "x-api-key: " + m_apiKey;
    headers = curl_slist_append(headers, keyHeader.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string response;
    setupCommonCurl(curl, url, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    ProgressContext pCtx = { progressCallback, callbackUser };
    if (progressCallback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pCtx);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    Logger::info("Uploading %s (%zu bytes) to %s...", photo.filename.c_str(), photo.fileSize, url.c_str());
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        Logger::error("Upload failed: %s", m_lastError.c_str());
        curl_slist_free_all(headers);
        curl_mime_free(mime);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 401) return ImmichError::UNAUTHORIZED;
    if (m_lastHttpCode == 403) {
        m_lastError = "Forbidden: API Key lacks 'asset.upload' permission";
        return ImmichError::FORBIDDEN;
    }

    if (m_lastHttpCode != 200 && m_lastHttpCode != 201) {
        m_lastError = "Server returned HTTP " + std::to_string(m_lastHttpCode) + ": " + response;
        Logger::error("%s", m_lastError.c_str());
        return ImmichError::SERVER_ERROR;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (root) {
        cJSON* idVal = cJSON_GetObjectItem(root, "id");
        if (idVal && cJSON_IsString(idVal) && idVal->valuestring) {
            outAssetId = idVal->valuestring;
        }
        cJSON* dupVal = cJSON_GetObjectItem(root, "duplicate");
        if (dupVal && cJSON_IsBool(dupVal)) {
            outDuplicate = cJSON_IsTrue(dupVal);
        }
        cJSON_Delete(root);
    }

    Logger::info("Upload successful! Asset ID: %s (Duplicate: %s)",
                 outAssetId.c_str(), outDuplicate ? "true" : "false");
    return ImmichError::SUCCESS;
}

ImmichError ImmichClient::searchAssets(int page, int size, std::vector<RemoteAsset>& outAssets, int& outTotal) {
    m_lastError.clear();
    m_lastHttpCode = 0;
    outAssets.clear();
    outTotal = 0;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "page", page);
    cJSON_AddNumberToObject(root, "size", size);
    cJSON_AddStringToObject(root, "order", "desc");

    char* postBody = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!postBody) return ImmichError::OUT_OF_MEMORY;

    CURL* curl = curl_easy_init();
    if (!curl) {
        cJSON_free(postBody);
        return ImmichError::NETWORK_ERROR;
    }

    std::string url = m_serverUrl + "/api/search/metadata";
    struct curl_slist* headers = nullptr;
    std::string keyHeader = "x-api-key: " + m_apiKey;
    headers = curl_slist_append(headers, keyHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string response;
    setupCommonCurl(curl, url, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    cJSON_free(postBody);

    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 401) return ImmichError::UNAUTHORIZED;
    if (m_lastHttpCode == 403) return ImmichError::FORBIDDEN;
    if (m_lastHttpCode != 200) {
        m_lastError = "HTTP " + std::to_string(m_lastHttpCode);
        return ImmichError::HTTP_ERROR;
    }

    cJSON* respJson = cJSON_Parse(response.c_str());
    if (!respJson) return ImmichError::INVALID_JSON;

    cJSON* assetsObj = cJSON_GetObjectItem(respJson, "assets");
    if (assetsObj && cJSON_IsObject(assetsObj)) {
        cJSON* totalVal = cJSON_GetObjectItem(assetsObj, "total");
        if (totalVal && cJSON_IsNumber(totalVal)) outTotal = totalVal->valueint;

        cJSON* itemsArray = cJSON_GetObjectItem(assetsObj, "items");
        if (itemsArray && cJSON_IsArray(itemsArray)) {
            cJSON* item;
            cJSON_ArrayForEach(item, itemsArray) {
                RemoteAsset ra;
                cJSON* v = cJSON_GetObjectItem(item, "id");
                if (v && cJSON_IsString(v) && v->valuestring) ra.id = v->valuestring;

                v = cJSON_GetObjectItem(item, "originalFileName");
                if (v && cJSON_IsString(v) && v->valuestring) ra.filename = v->valuestring;

                v = cJSON_GetObjectItem(item, "fileCreatedAt");
                if (v && cJSON_IsString(v) && v->valuestring) ra.createdAt = v->valuestring;

                v = cJSON_GetObjectItem(item, "type");
                if (v && cJSON_IsString(v) && v->valuestring) ra.type = v->valuestring;

                outAssets.push_back(ra);
            }
        }
    }

    cJSON_Delete(respJson);
    return ImmichError::SUCCESS;
}

ImmichError ImmichClient::getAssetThumbnail(const std::string& assetId, const std::string& size, std::vector<uint8_t>& outBytes) {
    m_lastError.clear();
    m_lastHttpCode = 0;
    outBytes.clear();

    CURL* curl = curl_easy_init();
    if (!curl) return ImmichError::NETWORK_ERROR;

    std::string url = m_serverUrl + "/api/assets/" + assetId + "/thumbnail?size=" + size;
    struct curl_slist* headers = nullptr;
    std::string keyHeader = "x-api-key: " + m_apiKey;
    headers = curl_slist_append(headers, keyHeader.c_str());

    setupCommonCurl(curl, url, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, binaryWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBytes);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        m_lastError = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return ImmichError::NETWORK_ERROR;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_lastHttpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (m_lastHttpCode == 401) return ImmichError::UNAUTHORIZED;
    if (m_lastHttpCode == 403) return ImmichError::FORBIDDEN;
    if (m_lastHttpCode != 200) {
        m_lastError = "HTTP " + std::to_string(m_lastHttpCode);
        return ImmichError::HTTP_ERROR;
    }

    return ImmichError::SUCCESS;
}
