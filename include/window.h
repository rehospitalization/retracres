#pragma once

#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#include <winuser.h>

#include "color.h"
#include "configuration.h"
#include "crosshair.h"
#include "display.h"
#include "fonts.h"
#include "overlay.h"
#include "resources.h"
#include "tips.h"
#include "ui.h"

#define WINDOW_STYLE (WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN)
#define WINDOW_EXSTYLE (WS_EX_APPWINDOW | WS_EX_WINDOWEDGE)
#define WM_USER_UPDATE_UI (WM_USER + 1)

class Window {
public:
    Window(HINSTANCE);
    ~Window();

    bool create(const wchar_t*, int, int);
    void center(DWORD, DWORD);
    void show(int);
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK FrameProc(HWND, UINT, WPARAM, LPARAM);

private:
    HWND hWnd = nullptr;
    HWND frameOverlay = nullptr;
    HINSTANCE hInstance = nullptr;
    HFONT hFont = nullptr;
    HFONT hFontSmall = nullptr;
    HFONT hFontTitle = nullptr;
    HFONT hFontButton = nullptr;
    HBRUSH hBrushBackground = nullptr;

    int title_hover = 0; // 0 none, 1 min, 2 close
    bool use_dwm_corners = false;

    Configuration* configuration = nullptr;
    CrosshairSettings* crosshair = nullptr;
    Overlay* overlay = nullptr;
    DisplayMode* display = nullptr;
    AppFonts* fonts = nullptr;
    UserInterface* ui = nullptr;

    static Window* getObject(HWND);
    void registerClass(const wchar_t*);
    void registerFrameClass();
    void createFrameOverlay();
    void layoutFrameOverlay();
    void paintFrameOverlay(HDC hdc) const;
    void applyRoundedCorners();
    void updateFallbackRegion();
    void paintTitleBar(HDC hdc);
    int hitTitleControls(POINT clientPt) const;
    RECT titleControlRect(int which) const;
    void applySettings();
    void revertSettings();
    bool applyDisplayForMode(int width, int height, int mode);
    void maybeShowBorderlessTip(int mode);
    void startOverlay();
    void stopOverlay();
    void syncOverlayFromUi(bool reloadImage);
    void persistCrosshair();
    void pickCrosshairColor();
    void loadCrosshairPng();
    void onCommand(WPARAM, LPARAM);
    void onCreate();
    void onDestroy();
};
