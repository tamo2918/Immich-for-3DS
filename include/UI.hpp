#ifndef UI_HPP
#define UI_HPP

#include <citro2d.h>
#include <string>
#include <vector>

enum class UIView {
    MAIN,
    PHOTOS,
    SETTINGS,
    GALLERY
};

struct TouchButton {
    float x, y, w, h;
    std::string label;
    u32 bgColor;
    u32 textColor;
    bool isPressed = false;

    bool contains(u16 px, u16 py) const {
        return (px >= x && px <= x + w && py >= y && py <= y + h);
    }
};

class UI {
public:
    UI();
    ~UI();

    bool init();
    void cleanup();

    void startFrame();
    void endFrame();

    // Top Screen Rendering
    void renderTopMain(const std::string& serverStatus, bool connected,
                       u8 wifiStrength, size_t totalPhotos, size_t unsyncedPhotos,
                       const std::string& lastSyncTime, const std::string& stateText,
                       float overallProgress, float fileProgress,
                       const std::string& currentFilename, const std::string& lastError);

    void renderTopPhotos(size_t total, size_t unsynced, const std::string& lastError);
    void renderTopSettings(const std::string& serverUrl, const std::string& maskedKey,
                           bool sslVerify, const std::string& testResult, bool testSuccess);
    void renderTopGallery(int page, int totalPages, int totalAssets);

    // Bottom Screen Rendering
    void renderBottomMain(const std::vector<TouchButton>& buttons);
    void renderBottomPhotos(const std::vector<std::string>& photoNames, size_t scrollOffset,
                            const std::vector<TouchButton>& buttons);
    void renderBottomSettings(const std::vector<TouchButton>& buttons, const std::string& statusMsg);
    void renderBottomGallery(const std::vector<TouchButton>& buttons, int page, int totalPages);

    // Text & Draw Helpers
    void drawText(float x, float y, float scale, u32 color, const char* format, ...);
    void drawRect(float x, float y, float w, float h, u32 color);
    void drawProgressBar(float x, float y, float w, float h, float progress, u32 fgColor, u32 bgColor);
    void drawButton(const TouchButton& btn);

private:
    C3D_RenderTarget* m_topTarget = nullptr;
    C3D_RenderTarget* m_bottomTarget = nullptr;
    C2D_TextBuf m_dynamicTextBuf = nullptr;
};

#endif // UI_HPP
