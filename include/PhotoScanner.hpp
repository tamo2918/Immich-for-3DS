#ifndef PHOTO_SCANNER_HPP
#define PHOTO_SCANNER_HPP

#include <string>
#include <vector>
#include <ctime>

enum class MediaType {
    PHOTO,
    VIDEO
};

struct PhotoInfo {
    std::string path;
    std::string filename;
    size_t fileSize = 0;
    time_t modTime = 0;
    std::string sha1; // Calculated when needed
    MediaType mediaType = MediaType::PHOTO;

    std::string getIsoTime() const;
};

using MediaInfo = PhotoInfo;

class PhotoScanner {
public:
    static bool isSupportedMedia(const std::string& path, MediaType& outType);
    static bool isSupportedPhoto(const std::string& path);
    static std::vector<PhotoInfo> scanDcim(const std::string& dcimRoot = "sdmc:/DCIM");
    static bool calculateSha1(PhotoInfo& photo);

private:
    static void scanDirectory(const std::string& dirPath, std::vector<PhotoInfo>& results);
};

#endif // PHOTO_SCANNER_HPP
