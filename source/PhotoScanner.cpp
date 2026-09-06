#include "PhotoScanner.hpp"
#include "sha1.h"
#include "Logger.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>

std::string PhotoInfo::getIsoTime() const {
    char buf[32];
    struct tm* tm_info = gmtime(&modTime);
    if (tm_info) {
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
        return std::string(buf);
    }
    return "2026-01-01T00:00:00Z";
}

bool PhotoScanner::isSupportedPhoto(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string ext = path.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }

    return (ext == ".jpg" || ext == ".jpeg");
}

void PhotoScanner::scanDirectory(const std::string& dirPath, std::vector<PhotoInfo>& results) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) {
        Logger::debug("Could not open directory: %s", dirPath.c_str());
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath.back() != '/') {
            fullPath += "/";
        }
        fullPath += entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectories (e.g. 100NIN03)
            scanDirectory(fullPath, results);
        } else if (S_ISREG(st.st_mode)) {
            std::string fname = entry->d_name;
            if (isSupportedPhoto(fname)) {
                PhotoInfo info;
                info.path = fullPath;
                info.filename = fname;
                info.fileSize = (size_t)st.st_size;
                info.modTime = st.st_mtime;
                results.push_back(info);
            }
        }
    }
    closedir(dir);
}

std::vector<PhotoInfo> PhotoScanner::scanDcim(const std::string& dcimRoot) {
    std::vector<PhotoInfo> photos;
    Logger::info("Scanning for photos in %s (JPEG only)...", dcimRoot.c_str());
    scanDirectory(dcimRoot, photos);

    // Sort by modification time ascending (chronological order)
    std::sort(photos.begin(), photos.end(), [](const PhotoInfo& a, const PhotoInfo& b) {
        if (a.modTime != b.modTime) {
            return a.modTime < b.modTime;
        }
        return a.filename < b.filename;
    });

    Logger::info("Found %zu photo(s) in %s", photos.size(), dcimRoot.c_str());
    return photos;
}

bool PhotoScanner::calculateSha1(PhotoInfo& photo) {
    if (!photo.sha1.empty()) return true;

    char hex[41];
    if (SHA1_FileHex(photo.path.c_str(), hex) == 0) {
        photo.sha1 = hex;
        return true;
    }
    Logger::error("Failed to compute SHA-1 for %s", photo.path.c_str());
    return false;
}
