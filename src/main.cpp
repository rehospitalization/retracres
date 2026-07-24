#include <windows.h>
#include <gdiplus.h>

#include "window.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    Window window(hInstance);

    bool success = window.create(L"RetracRes", 500, 720);

    if (!success) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    window.center(WINDOW_STYLE, WINDOW_EXSTYLE);
    window.show(nCmdShow);

    MSG msg = {};

    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}
