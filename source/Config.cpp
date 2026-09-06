#include "Config.hpp"
#include "cJSON.h"
#include "Logger.hpp"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#ifdef __3DS__
#include <3ds.h>
#endif

static void ensureDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = filepath.substr(0, pos);
#ifdef __3DS__
        mkdir(dir.c_str(), 0777);
#else
        mkdir(dir.c_str(), 0755);
#endif
    }
}

std::string ConfigManager::normalizeUrl(const std::string& url) {
    std::string norm = url;
    // Trim trailing slashes
    while (!norm.empty() && (norm.back() == '/' || norm.back() == ' ')) {
        norm.pop_back();
    }
    // Trim leading whitespace
    size_t start = norm.find_first_not_of(" ");
    if (start != std::string::npos) {
        norm = norm.substr(start);
    }
    return norm;
}

std::string ConfigManager::getMaskedApiKey(const std::string& key) {
    if (key.empty()) return "(Not set)";
    if (key.length() <= 8) return "********";
    return key.substr(0, 4) + "...." + key.substr(key.length() - 4);
}

bool ConfigManager::load(const std::string& path, AppConfig& config) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        Logger::info("Config file not found at %s. Using default settings.", path.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 1024 * 1024) {
        fclose(f);
        Logger::warn("Invalid config file size: %ld", sz);
        return false;
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
        Logger::error("Failed to parse config JSON: %s", path.c_str());
        return false;
    }

    cJSON* item = cJSON_GetObjectItem(root, "server_url");
    if (item && cJSON_IsString(item) && item->valuestring) {
        config.serverUrl = normalizeUrl(item->valuestring);
    }

    item = cJSON_GetObjectItem(root, "api_key");
    if (item && cJSON_IsString(item) && item->valuestring) {
        config.apiKey = item->valuestring;
    }

    item = cJSON_GetObjectItem(root, "ssl_verify");
    if (item && cJSON_IsBool(item)) {
        config.sslVerify = cJSON_IsTrue(item);
    }

    item = cJSON_GetObjectItem(root, "auto_sync");
    if (item && cJSON_IsBool(item)) {
        config.autoSync = cJSON_IsTrue(item);
    }

    item = cJSON_GetObjectItem(root, "sync_mpo");
    if (item && cJSON_IsBool(item)) {
        config.syncMpo = cJSON_IsTrue(item);
    }

    item = cJSON_GetObjectItem(root, "timeout_sec");
    if (item && cJSON_IsNumber(item)) {
        config.timeoutSec = item->valueint;
    }

    item = cJSON_GetObjectItem(root, "last_sync_time");
    if (item && cJSON_IsString(item) && item->valuestring) {
        config.lastSyncTime = item->valuestring;
    }

    cJSON_Delete(root);
    Logger::info("Loaded configuration: Server=%s, SSL_Verify=%s, LastSync=%s",
                 config.serverUrl.c_str(),
                 config.sslVerify ? "true" : "false",
                 config.lastSyncTime.c_str());
    return true;
}

bool ConfigManager::save(const std::string& path, const AppConfig& config) {
    ensureDirectory(path);

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "server_url", config.serverUrl.c_str());
    cJSON_AddStringToObject(root, "api_key", config.apiKey.c_str());
    cJSON_AddBoolToObject(root, "ssl_verify", config.sslVerify);
    cJSON_AddBoolToObject(root, "auto_sync", config.autoSync);
    cJSON_AddBoolToObject(root, "sync_mpo", config.syncMpo);
    cJSON_AddNumberToObject(root, "timeout_sec", config.timeoutSec);
    cJSON_AddStringToObject(root, "last_sync_time", config.lastSyncTime.c_str());

    char* jsonStr = cJSON_Print(root);
    cJSON_Delete(root);

    if (!jsonStr) return false;

    // Atomic save via temp file
    std::string tmpPath = path + ".tmp";
    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (!f) {
        cJSON_free(jsonStr);
        Logger::error("Failed to open temp config file for writing: %s", tmpPath.c_str());
        return false;
    }

    fputs(jsonStr, f);
    fclose(f);
    cJSON_free(jsonStr);

    remove(path.c_str());
    if (rename(tmpPath.c_str(), path.c_str()) != 0) {
        Logger::error("Failed to rename temp config file to %s", path.c_str());
        return false;
    }

    Logger::info("Saved configuration to %s", path.c_str());
    return true;
}
