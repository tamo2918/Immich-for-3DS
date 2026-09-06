#include "UI.hpp"
#include <cstdio>
#include <cstdarg>

#define COLOR_BG          C2D_Color32(20, 24, 32, 255)
#define COLOR_CARD        C2D_Color32(32, 38, 50, 255)
#define COLOR_HEADER      C2D_Color32(38, 86, 214, 255)
#define COLOR_TEXT        C2D_Color32(245, 245, 245, 255)
#define COLOR_TEXT_MUTED  C2D_Color32(160, 170, 185, 255)
#define COLOR_GREEN       C2D_Color32(46, 204, 113, 255)
#define COLOR_RED         C2D_Color32(231, 76, 60, 255)
#define COLOR_ORANGE      C2D_Color32(230, 126, 34, 255)
#define COLOR_BORDER      C2D_Color32(50, 60, 78, 255)

UI::UI() {
}

UI::~UI() {
    cleanup();
}

bool UI::init() {
#ifdef __3DS__
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    m_topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    m_dynamicTextBuf = C2D_TextBufNew(4096);
    return (m_topTarget && m_bottomTarget && m_dynamicTextBuf);
#else
    return true;
#endif
}

void UI::cleanup() {
#ifdef __3DS__
    if (m_dynamicTextBuf) {
        C2D_TextBufDelete(m_dynamicTextBuf);
        m_dynamicTextBuf = nullptr;
    }
    C2D_Fini();
    C3D_Fini();
    gfxExit();
#endif
}

void UI::startFrame() {
#ifdef __3DS__
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    if (m_dynamicTextBuf) {
        C2D_TextBufClear(m_dynamicTextBuf);
    }
#endif
}

void UI::endFrame() {
#ifdef __3DS__
    C3D_FrameEnd(0);
#endif
}

void UI::drawText(float x, float y, float scale, u32 color, const char* format, ...) {
#ifdef __3DS__
    if (!m_dynamicTextBuf) return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    C2D_Text c2dText;
    C2D_TextParse(&c2dText, m_dynamicTextBuf, buffer);
    C2D_TextOptimize(&c2dText);
    C2D_DrawText(&c2dText, C2D_WithColor, x, y, 0.5f, scale, scale, color);
#endif
}

void UI::drawRect(float x, float y, float w, float h, u32 color) {
#ifdef __3DS__
    C2D_DrawRectSolid(x, y, 0.4f, w, h, color);
#endif
}

void UI::drawProgressBar(float x, float y, float w, float h, float progress, u32 fgColor, u32 bgColor) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    drawRect(x, y, w, h, bgColor);
    if (progress > 0.0f) {
        drawRect(x, y, w * progress, h, fgColor);
    }
}

void UI::drawButton(const TouchButton& btn) {
    u32 bg = btn.isPressed ? C2D_Color32(28, 64, 160, 255) : btn.bgColor;
    drawRect(btn.x, btn.y, btn.w, btn.h, bg);
    drawRect(btn.x, btn.y, btn.w, 1.0f, COLOR_BORDER);
    drawRect(btn.x, btn.y + btn.h - 1.0f, btn.w, 1.0f, COLOR_BORDER);
    drawRect(btn.x, btn.y, 1.0f, btn.h, COLOR_BORDER);
    drawRect(btn.x + btn.w - 1.0f, btn.y, 1.0f, btn.h, COLOR_BORDER);

    float textX = btn.x + 12.0f;
    float textY = btn.y + (btn.h / 2.0f) - 7.0f;
    drawText(textX, textY, 0.50f, btn.textColor, "%s", btn.label.c_str());
}

void UI::renderTopMain(const std::string& serverStatus, bool connected,
                       u8 wifiStrength, size_t totalPhotos, size_t unsyncedPhotos,
                       const std::string& lastSyncTime, const std::string& stateText,
                       float overallProgress, float fileProgress,
                       const std::string& currentFilename, const std::string& lastError) {
#ifdef __3DS__
    C2D_TargetClear(m_topTarget, COLOR_BG);
    C2D_SceneBegin(m_topTarget);

    // Header bar
    drawRect(0, 0, 400, 26, COLOR_HEADER);
    drawText(10, 4, 0.55f, COLOR_TEXT, "Immich 3DS");

    // Wi-Fi signal icon
    const char* wifiIcons[] = { "[X]", "[.]", "[:]", "[|]" };
    const char* wifiStr = (wifiStrength <= 3) ? wifiIcons[wifiStrength] : "[?]";
    drawText(330, 5, 0.45f, COLOR_TEXT, "Wi-Fi: %s", wifiStr);

    // Server status card
    drawRect(15, 34, 370, 42, COLOR_CARD);
    u32 dotColor = connected ? COLOR_GREEN : COLOR_RED;
    drawRect(26, 48, 10, 10, dotColor);
    drawText(44, 44, 0.50f, COLOR_TEXT, "Server: %s", serverStatus.c_str());

    // Stats card
    drawRect(15, 84, 370, 78, COLOR_CARD);
    drawText(26, 92, 0.48f, COLOR_TEXT_MUTED, "Photos on 3DS:");
    drawText(160, 92, 0.50f, COLOR_TEXT, "%zu", totalPhotos);

    drawText(26, 114, 0.48f, COLOR_TEXT_MUTED, "Not Synced:");
    u32 unsyncedCol = (unsyncedPhotos > 0) ? COLOR_ORANGE : COLOR_GREEN;
    drawText(160, 114, 0.50f, unsyncedCol, "%zu", unsyncedPhotos);

    drawText(26, 136, 0.48f, COLOR_TEXT_MUTED, "Last Sync:");
    drawText(160, 136, 0.48f, COLOR_TEXT, "%s", lastSyncTime.c_str());

    // Sync progress / status area
    drawRect(15, 170, 370, 60, COLOR_CARD);
    if (!lastError.empty()) {
        drawText(26, 176, 0.45f, COLOR_RED, "Error: %s", lastError.c_str());
    } else if (!stateText.empty()) {
        drawText(26, 176, 0.48f, COLOR_GREEN, "%s", stateText.c_str());
        if (!currentFilename.empty()) {
            drawText(26, 194, 0.42f, COLOR_TEXT_MUTED, "File: %s", currentFilename.c_str());
            drawProgressBar(26, 212, 348, 8, fileProgress, COLOR_HEADER, COLOR_BG);
        }
    } else {
        drawText(26, 185, 0.45f, COLOR_TEXT_MUTED, "Ready. Press [Sync Now] or (A) to synchronize.");
    }
#endif
}

void UI::renderTopPhotos(size_t total, size_t unsynced, const std::string& lastError) {
#ifdef __3DS__
    C2D_TargetClear(m_topTarget, COLOR_BG);
    C2D_SceneBegin(m_topTarget);

    drawRect(0, 0, 400, 26, COLOR_HEADER);
    drawText(10, 4, 0.55f, COLOR_TEXT, "Photo Manager (DCIM)");

    drawRect(15, 36, 370, 50, COLOR_CARD);
    drawText(26, 44, 0.48f, COLOR_TEXT, "Detected on SD Card: %zu photos", total);
    drawText(26, 64, 0.48f, (unsynced > 0) ? COLOR_ORANGE : COLOR_GREEN, "Pending Upload: %zu photos", unsynced);

    drawRect(15, 96, 370, 130, COLOR_CARD);
    drawText(26, 106, 0.45f, COLOR_TEXT_MUTED, "Supported formats:");
    drawText(26, 126, 0.45f, COLOR_TEXT, "- .JPG (Standard 2D Camera Photos)");
    drawText(26, 146, 0.45f, COLOR_TEXT, "- .MPO (Nintendo 3DS 3D Photos)");
    drawText(26, 176, 0.42f, COLOR_TEXT_MUTED, "Immich stores and preserves both formats natively.");
#endif
}

void UI::renderTopSettings(const std::string& serverUrl, const std::string& maskedKey,
                           bool sslVerify, const std::string& testResult, bool testSuccess) {
#ifdef __3DS__
    C2D_TargetClear(m_topTarget, COLOR_BG);
    C2D_SceneBegin(m_topTarget);

    drawRect(0, 0, 400, 26, COLOR_HEADER);
    drawText(10, 4, 0.55f, COLOR_TEXT, "Settings & Connection Test");

    drawRect(15, 34, 370, 100, COLOR_CARD);
    drawText(26, 42, 0.45f, COLOR_TEXT_MUTED, "Immich Server URL:");
    drawText(26, 58, 0.48f, COLOR_TEXT, "%s", serverUrl.c_str());

    drawText(26, 80, 0.45f, COLOR_TEXT_MUTED, "API Key:");
    drawText(26, 96, 0.48f, COLOR_TEXT, "%s", maskedKey.c_str());

    drawText(26, 116, 0.45f, COLOR_TEXT_MUTED, "SSL Verification: %s", sslVerify ? "Enabled (Strict)" : "Disabled (Insecure)");

    // Connection test status card
    drawRect(15, 142, 370, 88, COLOR_CARD);
    drawText(26, 150, 0.48f, COLOR_TEXT, "Connection Test Result:");
    if (!testResult.empty()) {
        u32 col = testSuccess ? COLOR_GREEN : COLOR_RED;
        drawText(26, 174, 0.45f, col, "%s", testResult.c_str());
    } else {
        drawText(26, 174, 0.45f, COLOR_TEXT_MUTED, "Tap [Test Connection] below to verify setup.");
    }
#endif
}

void UI::renderTopGallery(int page, int totalPages, int totalAssets) {
#ifdef __3DS__
    C2D_TargetClear(m_topTarget, COLOR_BG);
    C2D_SceneBegin(m_topTarget);

    drawRect(0, 0, 400, 26, COLOR_HEADER);
    drawText(10, 4, 0.55f, COLOR_TEXT, "Immich Gallery (Page %d / %d)", page, (totalPages > 0 ? totalPages : 1));

    drawRect(15, 36, 370, 190, COLOR_CARD);
    drawText(26, 48, 0.50f, COLOR_TEXT, "Total Server Assets: %d", totalAssets);
    drawText(26, 74, 0.45f, COLOR_TEXT_MUTED, "Optimized for Old 3DS (Paging 20 items / page)");
    drawText(26, 98, 0.45f, COLOR_TEXT_MUTED, "Downloaded as compact thumbnails.");
#endif
}

void UI::renderBottomMain(const std::vector<TouchButton>& buttons) {
#ifdef __3DS__
    C2D_TargetClear(m_bottomTarget, COLOR_BG);
    C2D_SceneBegin(m_bottomTarget);

    for (const auto& btn : buttons) {
        drawButton(btn);
    }

    // Quick key guides at bottom
    drawText(12, 218, 0.42f, COLOR_TEXT_MUTED, "(A) Sync  (X) Photos  (Y) Config  (START) Quit");
#endif
}

void UI::renderBottomPhotos(const std::vector<std::string>& photoNames, size_t scrollOffset,
                            const std::vector<TouchButton>& buttons) {
#ifdef __3DS__
    C2D_TargetClear(m_bottomTarget, COLOR_BG);
    C2D_SceneBegin(m_bottomTarget);

    drawRect(10, 10, 300, 140, COLOR_CARD);
    if (photoNames.empty()) {
        drawText(20, 60, 0.48f, COLOR_GREEN, "All photos synced with Immich!");
    } else {
        float y = 16.0f;
        for (size_t i = scrollOffset; i < photoNames.size() && i < scrollOffset + 6; i++) {
            drawText(20, y, 0.42f, COLOR_TEXT, "[ ] %s", photoNames[i].c_str());
            y += 20.0f;
        }
    }

    for (const auto& btn : buttons) {
        drawButton(btn);
    }
#endif
}

void UI::renderBottomSettings(const std::vector<TouchButton>& buttons, const std::string& statusMsg) {
#ifdef __3DS__
    C2D_TargetClear(m_bottomTarget, COLOR_BG);
    C2D_SceneBegin(m_bottomTarget);

    for (const auto& btn : buttons) {
        drawButton(btn);
    }

    if (!statusMsg.empty()) {
        drawText(14, 150, 0.45f, COLOR_TEXT, "%s", statusMsg.c_str());
    }

    drawText(14, 218, 0.40f, COLOR_TEXT_MUTED, "Config stored at: sdmc:/3ds/Immich3DS/config.json");
#endif
}

void UI::renderBottomGallery(const std::vector<TouchButton>& buttons, int page, int totalPages) {
#ifdef __3DS__
    C2D_TargetClear(m_bottomTarget, COLOR_BG);
    C2D_SceneBegin(m_bottomTarget);

    for (const auto& btn : buttons) {
        drawButton(btn);
    }
#endif
}
