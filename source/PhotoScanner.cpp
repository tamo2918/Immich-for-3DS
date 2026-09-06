#include "PhotoScanner.hpp"
#include "sha1.h"
#include "Logger.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>

std::string PhotoInfo::getIsoTime() const {
    char buf[32];
    time_t t = modTime;
    // If modTime is 0, invalid, or pre-2000 (FAT epoch 1980 or Unix epoch 1970),
    // fallback to current time so the media is not lost in 1970 on the Immich timeline.
    if (t <= 946684800) {
        t = time(nullptr);
    }
    struct tm* tm_info = gmtime(&t);
    if (tm_info) {
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
        return std::string(buf);
    }
    return "2026-01-01T00:00:00Z";
}

bool PhotoScanner::isSupportedMedia(const std::string& path, MediaType& outType) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string ext = path.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".jpg" || ext == ".jpeg") {
        outType = MediaType::PHOTO;
        return true;
    } else if (ext == ".avi") {
        outType = MediaType::VIDEO;
        return true;
    }

    return false;
}

bool PhotoScanner::isSupportedPhoto(const std::string& path) {
    MediaType type;
    return isSupportedMedia(path, type) && (type == MediaType::PHOTO);
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
            MediaType mType;
            if (isSupportedMedia(fname, mType)) {
                PhotoInfo info;
                info.path = fullPath;
                info.filename = fname;
                info.fileSize = (size_t)st.st_size;
                info.modTime = st.st_mtime;
                info.mediaType = mType;
                results.push_back(info);
            }
        }
    }
    closedir(dir);
}

std::vector<PhotoInfo> PhotoScanner::scanDcim(const std::string& dcimRoot) {
    std::vector<PhotoInfo> media;
    Logger::info("Scanning for photos and videos in %s...", dcimRoot.c_str());
    scanDirectory(dcimRoot, media);

    // Sort by modification time ascending (chronological order)
    std::sort(media.begin(), media.end(), [](const PhotoInfo& a, const PhotoInfo& b) {
        if (a.modTime != b.modTime) {
            return a.modTime < b.modTime;
        }
        return a.filename < b.filename;
    });

    Logger::info("Found %zu media file(s) in %s", media.size(), dcimRoot.c_str());
    return media;
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
