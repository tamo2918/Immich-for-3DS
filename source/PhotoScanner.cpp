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

bool PhotoScanner::isSupportedExtension(const std::string& filename, bool includeMpo) {
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string ext = filename.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".jpg" || ext == ".jpeg") return true;
    if (includeMpo && ext == ".mpo") return true;

    return false;
}

void PhotoScanner::scanDirectory(const std::string& dirPath, std::vector<PhotoInfo>& results, bool includeMpo) {
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
            scanDirectory(fullPath, results, includeMpo);
        } else if (S_ISREG(st.st_mode)) {
            std::string fname = entry->d_name;
            if (isSupportedExtension(fname, includeMpo)) {
                PhotoInfo info;
                info.path = fullPath;
                info.filename = fname;
                info.fileSize = (size_t)st.st_size;
                info.modTime = st.st_mtime;

                size_t dot = fname.find_last_of('.');
                if (dot != std::string::npos) {
                    std::string ext = fname.substr(dot);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    info.isMpo = (ext == ".mpo");
                }

                results.push_back(info);
            }
        }
    }
    closedir(dir);
}

std::vector<PhotoInfo> PhotoScanner::scanDcim(const std::string& dcimRoot, bool includeMpo) {
    std::vector<PhotoInfo> photos;
    Logger::info("Scanning for photos in %s (includeMPO=%s)...", dcimRoot.c_str(), includeMpo ? "yes" : "no");
    scanDirectory(dcimRoot, photos, includeMpo);

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
