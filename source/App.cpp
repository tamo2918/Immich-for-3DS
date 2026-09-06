#include "App.hpp"
#include "Logger.hpp"

#ifdef __3DS__
#include <3ds.h>
#endif

App::App() {
}

App::~App() {
    cleanup();
}

void App::setupButtons() {
    // Main View Buttons
    m_mainButtons.clear();
    // 1. Sync Now (Primary)
    m_mainButtons.push_back({ 20, 15, 280, 44, "  Sync Now", C2D_Color32(38, 86, 214, 255), C2D_Color32(255, 255, 255, 255) });
    // 2. Photos List
    m_mainButtons.push_back({ 20, 68, 280, 38, "  Photos on SD Card", C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    // 3. Settings
    m_mainButtons.push_back({ 20, 114, 135, 38, "  Settings", C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    // 4. Gallery
    m_mainButtons.push_back({ 165, 114, 135, 38, "  Gallery", C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    // 5. Exit
    m_mainButtons.push_back({ 20, 160, 280, 36, "  Exit Immich 3DS", C2D_Color32(70, 30, 30, 255), C2D_Color32(255, 200, 200, 255) });

    // Photos View Buttons
    m_photoButtons.clear();
    m_photoButtons.push_back({ 20, 165, 135, 38, "  Sync All", C2D_Color32(38, 86, 214, 255), C2D_Color32(255, 255, 255, 255) });
    m_photoButtons.push_back({ 165, 165, 135, 38, "  Back", C2D_Color32(50, 58, 74, 255), C2D_Color32(240, 240, 240, 255) });

    // Settings View Buttons
    m_settingsButtons.clear();
    m_settingsButtons.push_back({ 20, 18, 280, 40, "  Test Connection", C2D_Color32(38, 86, 214, 255), C2D_Color32(255, 255, 255, 255) });
    std::string sslLabel = m_config.sslVerify ? "  SSL Verify: ON" : "  SSL Verify: OFF (Insecure)";
    m_settingsButtons.push_back({ 20, 68, 280, 40, sslLabel, C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    m_settingsButtons.push_back({ 20, 165, 280, 38, "  Back to Main Menu", C2D_Color32(50, 58, 74, 255), C2D_Color32(240, 240, 240, 255) });

    // Gallery View Buttons
    m_galleryButtons.clear();
    m_galleryButtons.push_back({ 20, 165, 80, 38, "  Prev", C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    m_galleryButtons.push_back({ 110, 165, 80, 38, "  Next", C2D_Color32(40, 48, 64, 255), C2D_Color32(240, 240, 240, 255) });
    m_galleryButtons.push_back({ 200, 165, 100, 38, "  Back", C2D_Color32(50, 58, 74, 255), C2D_Color32(240, 240, 240, 255) });
}

bool App::init() {
    Logger::init("sdmc:/3ds/Immich3DS/log.txt");
    Logger::info("Initializing Immich 3DS...");

    // Load config (or save default template if not found)
    if (!ConfigManager::load("sdmc:/3ds/Immich3DS/config.json", m_config)) {
        ConfigManager::save("sdmc:/3ds/Immich3DS/config.json", m_config);
    }

    // Initialize UI
    if (!m_ui.init()) {
        Logger::error("Failed to initialize Citro2D UI");
        return false;
    }

    // Initialize Client & Sync Manager
    ImmichClient::globalInit();
    m_client = std::make_unique<ImmichClient>(m_config.serverUrl, m_config.apiKey, m_config.sslVerify, m_config.timeoutSec);
    m_syncMgr = std::make_unique<SyncManager>(*m_client, m_config, "sdmc:/3ds/Immich3DS/sync.json");

    m_syncMgr->loadSyncDb();
    m_syncMgr->scanLocalPhotos();

    setupButtons();

    // Initial connection test
    testConnection();

    // Run auto-sync if enabled
    if (m_config.autoSync && m_connected) {
        Logger::info("Auto-sync is enabled. Starting initial sync...");
        startSync();
    }

    return true;
}

void App::cleanup() {
#ifdef __3DS__
    if (m_syncThread) {
        if (m_syncMgr) m_syncMgr->cancelSync();
        threadJoin(m_syncThread, U64_MAX);
        threadFree(m_syncThread);
        m_syncThread = nullptr;
    }
#endif
    ImmichClient::globalCleanup();
    m_ui.cleanup();
    Logger::info("Immich 3DS cleaned up.");
    Logger::close();
}

void App::testConnection() {
    m_testResult = "Connecting to server...";
    m_testSuccess = false;
    m_serverStatus = "Checking...";

    // 1. Ping
    std::string pingMsg;
    ImmichError err = m_client->ping(pingMsg);
    if (err != ImmichError::SUCCESS) {
        m_connected = false;
        m_serverStatus = "Disconnected";
        m_testResult = "Ping failed: " + m_client->getLastError();
        Logger::warn("Connection test failed: %s", m_testResult.c_str());
        return;
    }

    // 2. Version
    ServerVersion ver;
    err = m_client->getVersion(ver);
    std::string verStr = (err == ImmichError::SUCCESS) ? ver.toString() : "Unknown";

    // 3. Auth (Me)
    UserInfo user;
    err = m_client->getMe(user);
    if (err == ImmichError::UNAUTHORIZED) {
        m_connected = true; // Server is reachable, but key is invalid
        m_serverStatus = "Invalid API Key";
        m_testResult = "Server reachable (" + verStr + ") but API Key is invalid!";
        return;
    } else if (err == ImmichError::FORBIDDEN) {
        m_connected = true;
        m_serverStatus = "Connected (Scoped)";
        m_testResult = "Connected to " + verStr + " (API key lacks user.read scope)";
        m_testSuccess = true;
        return;
    } else if (err == ImmichError::SUCCESS) {
        m_connected = true;
        m_serverStatus = "Connected";
        m_testResult = "Connected! Immich " + verStr + "\nUser: " + user.name + " (" + user.email + ")";
        m_testSuccess = true;
        Logger::info("Connection test passed: %s (%s)", verStr.c_str(), user.name.c_str());
        return;
    }

    m_connected = true;
    m_serverStatus = "Connected";
    m_testResult = "Connected to Immich " + verStr;
    m_testSuccess = true;
}

#ifdef __3DS__
void App::syncThreadFunc(void* arg) {
    App* app = static_cast<App*>(arg);
    if (app && app->m_syncMgr) {
        app->m_syncMgr->performSync();
    }
    if (app) {
        app->m_syncInProgress = false;
    }
    threadExit(0);
}
#endif

void App::startSync() {
    if (m_syncInProgress) {
        Logger::info("Cancel requested by user");
        m_syncMgr->cancelSync();
        return;
    }
    if (m_syncMgr->getState() != SyncState::IDLE && m_syncMgr->getState() != SyncState::COMPLETED && m_syncMgr->getState() != SyncState::FAILED) {
        return;
    }

#ifdef __3DS__
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    int workerPrio = (prio < 0x3E) ? prio + 1 : 0x3E;
    m_syncInProgress = true;
    m_syncThread = threadCreate(syncThreadFunc, this, 64 * 1024, workerPrio, -2, false);
    if (!m_syncThread) {
        m_syncInProgress = false;
        Logger::error("Failed to create worker thread, running synchronously");
        runSync();
    }
#else
    m_syncInProgress = true;
    runSync();
    m_syncInProgress = false;
#endif
}

void App::runSync() {
    m_syncMgr->performSync();
}

void App::updateNetworkState() {
#ifdef __3DS__
    m_wifiStrength = osGetWifiStrength();
#else
    m_wifiStrength = 3;
#endif
}

void App::handleInput() {
#ifdef __3DS__
    hidScanInput();
    u32 kDown = hidKeysDown();
    touchPosition touch;
    hidTouchRead(&touch);

    if (kDown & KEY_START) {
        m_running = false;
        return;
    }

    // View-specific input handling
    if (m_currentView == UIView::MAIN) {
        if (kDown & KEY_A) {
            startSync();
        } else if (kDown & KEY_X) {
            m_currentView = UIView::PHOTOS;
        } else if (kDown & KEY_Y) {
            m_currentView = UIView::SETTINGS;
        }

        if (kDown & KEY_TOUCH) {
            if (m_mainButtons.size() > 0 && m_mainButtons[0].contains(touch.px, touch.py)) { // Sync Now / Cancel Sync
                startSync();
            } else if (m_mainButtons.size() > 1 && m_mainButtons[1].contains(touch.px, touch.py)) { // Photos
                m_currentView = UIView::PHOTOS;
            } else if (m_mainButtons.size() > 2 && m_mainButtons[2].contains(touch.px, touch.py)) { // Settings
                m_currentView = UIView::SETTINGS;
            } else if (m_mainButtons.size() > 3 && m_mainButtons[3].contains(touch.px, touch.py)) { // Gallery
                m_currentView = UIView::GALLERY;
            } else if (m_mainButtons.size() > 4 && m_mainButtons[4].contains(touch.px, touch.py)) { // Exit
                m_running = false;
            }
        }
    } else if (m_currentView == UIView::PHOTOS) {
        if (kDown & KEY_B) {
            m_currentView = UIView::MAIN;
        } else if (kDown & KEY_A) {
            m_currentView = UIView::MAIN;
            startSync();
        }

        if (kDown & KEY_TOUCH) {
            if (m_photoButtons.size() > 0 && m_photoButtons[0].contains(touch.px, touch.py)) { // Sync All
                m_currentView = UIView::MAIN;
                startSync();
            } else if (m_photoButtons.size() > 1 && m_photoButtons[1].contains(touch.px, touch.py)) { // Back
                m_currentView = UIView::MAIN;
            }
        }
    } else if (m_currentView == UIView::SETTINGS) {
        if (kDown & KEY_B) {
            m_currentView = UIView::MAIN;
        } else if (kDown & KEY_A) {
            testConnection();
        }

        if (kDown & KEY_TOUCH) {
            if (m_settingsButtons.size() > 0 && m_settingsButtons[0].contains(touch.px, touch.py)) { // Test Connection
                testConnection();
            } else if (m_settingsButtons.size() > 1 && m_settingsButtons[1].contains(touch.px, touch.py)) { // Toggle SSL
                m_config.sslVerify = !m_config.sslVerify;
                m_client->updateConfig(m_config.serverUrl, m_config.apiKey, m_config.sslVerify, m_config.timeoutSec);
                ConfigManager::save("sdmc:/3ds/Immich3DS/config.json", m_config);
                m_settingsButtons[1].label = m_config.sslVerify ? "  SSL Verify: ON" : "  SSL Verify: OFF (Insecure)";
                m_settingsStatus = m_config.sslVerify ? "SSL Verification enabled." : "SSL Verification disabled (allows self-signed).";
            } else if (m_settingsButtons.size() > 2 && m_settingsButtons[2].contains(touch.px, touch.py)) { // Back
                m_currentView = UIView::MAIN;
            }
        }
    } else if (m_currentView == UIView::GALLERY) {
        if (kDown & KEY_B) {
            m_currentView = UIView::MAIN;
        }

        if (kDown & KEY_TOUCH) {
            if (m_galleryButtons.size() > 2 && m_galleryButtons[2].contains(touch.px, touch.py)) { // Back
                m_currentView = UIView::MAIN;
            }
        }
    }
#endif
}

void App::run() {
    u32 frameCount = 0;
    while (m_running) {
#ifdef __3DS__
        if (!aptMainLoop()) break;
#endif

        handleInput();

#ifdef __3DS__
        if (m_syncThread && !m_syncInProgress) {
            threadJoin(m_syncThread, U64_MAX);
            threadFree(m_syncThread);
            m_syncThread = nullptr;
        }
#endif

        if (++frameCount % 60 == 0) {
            updateNetworkState();
        }

        // Dynamically update Sync button label & color on main screen
        if (!m_mainButtons.empty()) {
            if (m_syncInProgress) {
                m_mainButtons[0].label = "  Cancel Sync";
                m_mainButtons[0].bgColor = C2D_Color32(180, 40, 40, 255);
            } else {
                m_mainButtons[0].label = "  Sync Now";
                m_mainButtons[0].bgColor = C2D_Color32(38, 86, 214, 255);
            }
        }

        // Render Frame
        m_ui.startFrame();

        std::string stateText;
        SyncState st = m_syncMgr->getState();
        if (st == SyncState::SCANNING) stateText = "Scanning SD Card for DCIM photos...";
        else if (st == SyncState::CALCULATING_HASH) stateText = "Computing SHA-1 photo hashes...";
        else if (st == SyncState::CHECKING_SERVER) stateText = "Checking duplicates with Immich...";
        else if (st == SyncState::UPLOADING) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Uploading %zu / %zu",
                     m_syncMgr->getCurrentSyncIndex(), m_syncMgr->getTotalSyncCount());
            stateText = buf;
        } else if (st == SyncState::COMPLETED) stateText = "All photos synchronized!";

        if (m_currentView == UIView::MAIN) {
            float overallProg = (m_syncMgr->getTotalSyncCount() > 0) ?
                (float)m_syncMgr->getCurrentSyncIndex() / (float)m_syncMgr->getTotalSyncCount() : 0.0f;

            m_ui.renderTopMain(m_serverStatus, m_connected, m_wifiStrength,
                               m_syncMgr->getTotalPhotosCount(),
                               m_syncMgr->getUnsyncedPhotosCount(),
                               m_config.lastSyncTime, stateText,
                               overallProg, m_syncMgr->getCurrentFileProgress(),
                               m_syncMgr->getCurrentFilename(), m_syncMgr->getLastError(),
                               frameCount, m_syncInProgress,
                               m_syncMgr->getCurrentFileNow(), m_syncMgr->getCurrentFileTotal());

            m_ui.renderBottomMain(m_mainButtons);
        } else if (m_currentView == UIView::PHOTOS) {
            m_ui.renderTopPhotos(m_syncMgr->getTotalPhotosCount(),
                                 m_syncMgr->getUnsyncedPhotosCount(),
                                 m_syncMgr->getLastError());

            std::vector<std::string> photoNames;
            for (const auto& p : m_syncMgr->getUnsyncedPhotos()) {
                photoNames.push_back(p.filename + (p.isMpo ? " [3D MPO]" : " [2D JPG]"));
            }
            m_ui.renderBottomPhotos(photoNames, m_photoScrollOffset, m_photoButtons);
        } else if (m_currentView == UIView::SETTINGS) {
            m_ui.renderTopSettings(m_config.serverUrl,
                                   ConfigManager::getMaskedApiKey(m_config.apiKey),
                                   m_config.sslVerify, m_testResult, m_testSuccess);

            m_ui.renderBottomSettings(m_settingsButtons, m_settingsStatus);
        } else if (m_currentView == UIView::GALLERY) {
            m_ui.renderTopGallery(m_galleryPage, m_galleryTotalPages, m_galleryTotalAssets);
            m_ui.renderBottomGallery(m_galleryButtons, m_galleryPage, m_galleryTotalPages);
        }

        m_ui.endFrame();
    }
}
