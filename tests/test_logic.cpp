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

    // Unit test isSupportedMedia & isSupportedPhoto
    MediaType type;
    assert(PhotoScanner::isSupportedMedia("HNI_0001.JPG", type) == true && type == MediaType::PHOTO);
    assert(PhotoScanner::isSupportedMedia("HNI_0001.jpg", type) == true && type == MediaType::PHOTO);
    assert(PhotoScanner::isSupportedMedia("photo.jpeg", type) == true && type == MediaType::PHOTO);
    assert(PhotoScanner::isSupportedMedia("photo.JPEG", type) == true && type == MediaType::PHOTO);
    assert(PhotoScanner::isSupportedMedia("HNI_0003.AVI", type) == true && type == MediaType::VIDEO);
    assert(PhotoScanner::isSupportedMedia("video.avi", type) == true && type == MediaType::VIDEO);

    // MPO must always return false
    assert(PhotoScanner::isSupportedMedia("HNI_0001.MPO", type) == false);
    assert(PhotoScanner::isSupportedMedia("HNI_0001.mpo", type) == false);

    // Other unsupported formats
    assert(PhotoScanner::isSupportedMedia("video.mp4", type) == false);
    assert(PhotoScanner::isSupportedMedia("image.png", type) == false);
    assert(PhotoScanner::isSupportedMedia("no_ext", type) == false);

    // Backward compatibility check
    assert(PhotoScanner::isSupportedPhoto("HNI_0001.JPG") == true);
    assert(PhotoScanner::isSupportedPhoto("HNI_0001.AVI") == false);
    assert(PhotoScanner::isSupportedPhoto("HNI_0001.MPO") == false);

    system("rm -rf tests/mock_dcim");
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

    FILE* f4 = fopen("tests/mock_dcim/100NIN03/HNI_0003.AVI", "wb");
    assert(f4);
    fputs("mock 3ds mjpeg avi video 1", f4);
    fclose(f4);

    FILE* f5 = fopen("tests/mock_dcim/100NIN03/HNI_0004.avi", "wb");
    assert(f5);
    fputs("mock 3ds mjpeg avi video 2", f5);
    fclose(f5);

    FILE* f6 = fopen("tests/mock_dcim/100NIN03/HNI_0005.MP4", "wb");
    assert(f6);
    fputs("unsupported mp4", f6);
    fclose(f6);

    FILE* f7 = fopen("tests/mock_dcim/100NIN03/README.TXT", "wb");
    assert(f7);
    fputs("text file", f7);
    fclose(f7);

    // Scan mock DCIM: Must return exactly 2 photos and 2 videos (4 items total). MPO, MP4, TXT ignored.
    auto mediaList = PhotoScanner::scanDcim("tests/mock_dcim");
    assert(mediaList.size() == 4);

    int photoCount = 0;
    int videoCount = 0;
    for (auto& m : mediaList) {
        bool ok = PhotoScanner::calculateSha1(m);
        assert(ok);
        assert(!m.sha1.empty());
        if (m.mediaType == MediaType::PHOTO) photoCount++;
        else if (m.mediaType == MediaType::VIDEO) videoCount++;
    }
    assert(photoCount == 2);
    assert(videoCount == 2);

    system("rm -rf tests/mock_dcim");
    std::cout << "  PhotoScanner test passed (Photos: 2, Videos: 2, MPO/MP4 excluded)!" << std::endl;
}

void testSyncManager() {
    std::cout << "[TEST] Running SyncManager database tests..." << std::endl;
    AppConfig cfg;
    ImmichClient client("http://mock-server:2283", "mock-key");
    std::string dbPath = "tests/test_sync.json";

    // Setup mock DCIM with 1 photo and 1 video
    system("rm -rf tests/mock_dcim");
    system("mkdir -p tests/mock_dcim/100NIN03");
    FILE* f1 = fopen("tests/mock_dcim/100NIN03/HNI_0010.JPG", "wb");
    fputs("photo data", f1);
    fclose(f1);
    FILE* f2 = fopen("tests/mock_dcim/100NIN03/HNI_0011.AVI", "wb");
    fputs("video data", f2);
    fclose(f2);
    FILE* f3 = fopen("tests/mock_dcim/100NIN03/HNI_0010.MPO", "wb");
    fputs("mpo data", f3);
    fclose(f3);

    SyncManager syncMgr(client, cfg, dbPath);
    bool ok = syncMgr.loadSyncDb();
    assert(ok);

    syncMgr.scanLocalPhotos(); // Uses default DCIM search, but mock_dcim can be tested via scanner
    bool saved = syncMgr.saveSyncDb();
    assert(saved);

    // Create a mock sync.json with photo and video records
    cJSON* root = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();

    cJSON* pRec = cJSON_CreateObject();
    cJSON_AddStringToObject(pRec, "filename", "HNI_0001.JPG");
    cJSON_AddStringToObject(pRec, "media_type", "photo");
    cJSON_AddStringToObject(pRec, "sha1", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    cJSON_AddItemToArray(arr, pRec);

    cJSON* vRec = cJSON_CreateObject();
    cJSON_AddStringToObject(vRec, "filename", "HNI_0002.AVI");
    cJSON_AddStringToObject(vRec, "media_type", "video");
    cJSON_AddStringToObject(vRec, "sha1", "fb49a3ee5e6b4b0d3255bfef95601890afd80710");
    cJSON_AddItemToArray(arr, vRec);

    cJSON_AddItemToObject(root, "records", arr);
    char* jsonStr = cJSON_Print(root);
    FILE* outF = fopen(dbPath.c_str(), "w");
    assert(outF);
    fputs(jsonStr, outF);
    fclose(outF);
    free(jsonStr);
    cJSON_Delete(root);

    // Reload with a new SyncManager to verify media_type deserialization
    SyncManager reloadedMgr(client, cfg, dbPath);
    assert(reloadedMgr.loadSyncDb());
    assert(reloadedMgr.saveSyncDb());

    // Verify written file contains "video" and "photo" media_type
    FILE* checkF = fopen(dbPath.c_str(), "r");
    assert(checkF);
    char readBuf[1024];
    size_t n = fread(readBuf, 1, sizeof(readBuf) - 1, checkF);
    readBuf[n] = '\0';
    fclose(checkF);

    assert(strstr(readBuf, "\"media_type\":\t\"photo\"") != nullptr || strstr(readBuf, "\"media_type\": \"photo\"") != nullptr);
    assert(strstr(readBuf, "\"media_type\":\t\"video\"") != nullptr || strstr(readBuf, "\"media_type\": \"video\"") != nullptr);

    remove(dbPath.c_str());
    system("rm -rf tests/mock_dcim");

    std::cout << "  SyncManager test passed (photo & video records verified in sync.json)!" << std::endl;
}

void testIsoTime() {
    std::cout << "[TEST] Running PhotoInfo getIsoTime tests..." << std::endl;
    PhotoInfo p;
    // Test normal valid timestamp (2023-05-15 12:00:00 UTC = 1684152000)
    p.modTime = 1684152000;
    std::string iso = p.getIsoTime();
    assert(iso == "2023-05-15T12:00:00Z");

    // Test zero / invalid timestamp (1970-01-01) -> must fallback to current time, never 1970
    p.modTime = 0;
    std::string fallbackIso = p.getIsoTime();
    assert(fallbackIso.find("1970") == std::string::npos);
    assert(fallbackIso.find("1980") == std::string::npos);
    std::cout << "  Valid ISO: " << iso << ", Fallback ISO: " << fallbackIso << std::endl;
}

int main() {
    Logger::init("");
    testSha1();
    testConfig();
    testPhotoScanner();
    testSyncManager();
    testIsoTime();
    std::cout << "\n>>> ALL HOST LOGIC TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}

