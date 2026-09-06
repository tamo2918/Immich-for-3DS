#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/stat.h>

#include "cJSON.h"
#include "sha1.h"
#include "Config.hpp"
#include "PhotoScanner.hpp"
#include "Logger.hpp"
#include "ImmichClient.hpp"
#include "SyncManager.hpp"

void testSha1() {
    std::cout << "[TEST] Running SHA-1 tests..." << std::endl;
    FILE* f = fopen("tests/test_file.bin", "wb");
    assert(f != nullptr);
    const char* dummyData = "Hello Immich 3DS";
    fwrite(dummyData, 1, strlen(dummyData), f);
    fclose(f);

    char hex[41];
    int res = SHA1_FileHex("tests/test_file.bin", hex);
    assert(res == 0);
    assert(strlen(hex) == 40);
    std::cout << "  File SHA-1: " << hex << std::endl;
    remove("tests/test_file.bin");
}

void testConfig() {
    std::cout << "[TEST] Running ConfigManager tests..." << std::endl;
    AppConfig cfg;
    cfg.serverUrl = "http://192.168.1.50:2283/";
    cfg.apiKey = "abcdef1234567890abcdef1234567890";
    cfg.sslVerify = false;
    cfg.autoSync = true;
    cfg.syncMpo = true;
    cfg.timeoutSec = 20;
    cfg.lastSyncTime = "2026/09/06 21:00";

    std::string testPath = "tests/test_config.json";
    bool saved = ConfigManager::save(testPath, cfg);
    assert(saved);

    AppConfig loaded;
    bool ok = ConfigManager::load(testPath, loaded);
    assert(ok);
    assert(loaded.serverUrl == "http://192.168.1.50:2283"); // Normalized trailing slash
    assert(loaded.apiKey == cfg.apiKey);
    assert(loaded.sslVerify == false);
    assert(loaded.autoSync == true);
    assert(loaded.timeoutSec == 20);
    assert(loaded.lastSyncTime == "2026/09/06 21:00");

    std::string masked = ConfigManager::getMaskedApiKey(loaded.apiKey);
    assert(masked == "abcd....7890");

    remove(testPath.c_str());
    std::cout << "  Config test passed!" << std::endl;
}

void testPhotoScanner() {
    std::cout << "[TEST] Running PhotoScanner tests..." << std::endl;
    system("mkdir -p tests/mock_dcim/100NIN03");
    FILE* f1 = fopen("tests/mock_dcim/100NIN03/HNI_0001.JPG", "wb");
    assert(f1);
    fputs("mock jpg 1", f1);
    fclose(f1);

    FILE* f2 = fopen("tests/mock_dcim/100NIN03/HNI_0001.MPO", "wb");
    assert(f2);
    fputs("mock mpo 3d 1", f2);
    fclose(f2);

    FILE* f3 = fopen("tests/mock_dcim/100NIN03/HNI_0002.jpg", "wb");
    assert(f3);
    fputs("mock jpg 2", f3);
    fclose(f3);

    auto photos = PhotoScanner::scanDcim("tests/mock_dcim", true);
    assert(photos.size() == 3);

    bool foundJpg = false, foundMpo = false;
    for (auto& p : photos) {
        bool ok = PhotoScanner::calculateSha1(p);
        assert(ok);
        assert(!p.sha1.empty());
        if (p.filename == "HNI_0001.JPG") foundJpg = true;
        if (p.filename == "HNI_0001.MPO") {
            foundMpo = true;
            assert(p.isMpo == true);
        }
    }
    assert(foundJpg && foundMpo);

    auto photosNoMpo = PhotoScanner::scanDcim("tests/mock_dcim", false);
    assert(photosNoMpo.size() == 2);

    system("rm -rf tests/mock_dcim");
    std::cout << "  PhotoScanner test passed!" << std::endl;
}

void testSyncManager() {
    std::cout << "[TEST] Running SyncManager database tests..." << std::endl;
    AppConfig cfg;
    ImmichClient client("http://mock-server:2283", "mock-key");
    std::string dbPath = "tests/test_sync.json";

    SyncManager syncMgr(client, cfg, dbPath);
    bool ok = syncMgr.loadSyncDb();
    assert(ok);

    bool saved = syncMgr.saveSyncDb();
    assert(saved);

    FILE* f = fopen(dbPath.c_str(), "rb");
    assert(f != nullptr);
    fclose(f);
    remove(dbPath.c_str());

    std::cout << "  SyncManager test passed!" << std::endl;
}

int main() {
    Logger::init("");
    testSha1();
    testConfig();
    testPhotoScanner();
    testSyncManager();
    std::cout << "\n>>> ALL HOST LOGIC TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
