#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

struct AppConfig {
    std::string serverUrl = "http://192.168.1.100:2283";
    std::string apiKey = "";
    bool sslVerify = true;
    bool autoSync = false;
    int timeoutSec = 15;
    std::string lastSyncTime = "Never";
};

class ConfigManager {
public:
    static bool load(const std::string& path, AppConfig& config);
    static bool save(const std::string& path, const AppConfig& config);

    // Helpers
    static std::string getMaskedApiKey(const std::string& key);
    static std::string normalizeUrl(const std::string& url);
};

#endif // CONFIG_HPP
