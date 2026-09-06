#ifndef APP_HPP
#define APP_HPP

#include "Config.hpp"
#include "ImmichClient.hpp"
#include "SyncManager.hpp"
#include "UI.hpp"
#include <memory>

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void cleanup();

private:
    AppConfig m_config;
    std::unique_ptr<ImmichClient> m_client;
    std::unique_ptr<SyncManager> m_syncMgr;
    UI m_ui;

    UIView m_currentView = UIView::MAIN;
    bool m_running = true;
    bool m_connected = false;
    u8 m_wifiStrength = 0;
    std::string m_serverStatus = "Checking...";
    std::string m_testResult;
    bool m_testSuccess = false;
    std::string m_settingsStatus;

    // Photos View state
    size_t m_photoScrollOffset = 0;

    // Gallery View state
    int m_galleryPage = 1;
    int m_galleryTotalPages = 1;
    int m_galleryTotalAssets = 0;

    // Button sets
    std::vector<TouchButton> m_mainButtons;
    std::vector<TouchButton> m_photoButtons;
    std::vector<TouchButton> m_settingsButtons;
    std::vector<TouchButton> m_galleryButtons;

    void setupButtons();
    void handleInput();
    void updateNetworkState();
    void testConnection();
    void runSync();
};

#endif // APP_HPP
