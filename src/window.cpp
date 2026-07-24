#include "window.h"

#include <algorithm>
#include <commdlg.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <stdexcept>
#include <windowsx.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

namespace {
constexpr int kTitleHitNone = 0;
constexpr int kTitleHitMin = 1;
constexpr int kTitleHitClose = 2;
constexpr COLORREF kFrameColorKey = RGB(255, 0, 255);
constexpr wchar_t kFrameClassName[] = L"RetracResFrame";
}

Window::Window(HINSTANCE hInstance) : hWnd(nullptr), hInstance(hInstance) {
    this->configuration = new Configuration();
    this->crosshair = new CrosshairSettings();
    this->overlay = new Overlay();
    this->display = new DisplayMode();
    this->display->loadBackup();
    this->fonts = new AppFonts();
    this->fonts->load();
}

Window::~Window() {
    delete this->fonts;
    delete this->display;
    delete this->overlay;
    delete this->crosshair;
    delete this->configuration;
}

bool Window::applyDisplayForMode(int width, int height, int mode) {
    if (!this->display) {
        return true;
    }

    // Exclusive fullscreen lets Fortnite change the display mode itself.
    if (mode == MODE_FULLSCREEN) {
        if (this->display->hasBackup()) {
            this->display->restore();
        }
        return true;
    }

    // Borderless/Windowed need the Windows desktop at the target resolution
    // so Fortnite (and the crosshair overlay) actually run stretched.
    if (!this->display->hasBackup()) {
        this->display->captureCurrent();
    }

    if (!this->display->modeAvailable(width, height)) {
        MessageBoxW(
            this->hWnd,
            L"This resolution is not available as a Windows display mode.\n\n"
            L"Create it first in NVIDIA/AMD control panel (custom resolution), "
            L"set GPU scaling to Full screen / Full panel, then Apply again.\n\n"
            L"INI settings were still saved.",
            L"Display mode missing",
            MB_OK | MB_ICONWARNING
        );
        return false;
    }

    if (!this->display->apply(width, height)) {
        MessageBoxW(
            this->hWnd,
            L"Unable to switch Windows to the selected resolution.\n\n"
            L"INI settings were still saved. Check GPU custom resolutions and try again.",
            L"Display switch failed",
            MB_OK | MB_ICONWARNING
        );
        return false;
    }

    return true;
}

void Window::maybeShowBorderlessTip(int mode) {
    if (mode == MODE_FULLSCREEN) {
        return;
    }
    if (!Tips::shouldShowBorderlessTip()) {
        return;
    }
    Tips::showBorderlessTip(this->hWnd);
}

void Window::applySettings() {
    std::wstring path = this->configuration->getPath();
    LPCWSTR file = path.c_str();

    bool is_writeable = this->configuration->unsetReadOnlyAttribute(path);

    if (!is_writeable) {
        MessageBoxW(
            hWnd,
            L"Unable to make the configuration file writable.",
            L"Error",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    bool success = this->configuration->createBackup();

    if (!success) {
        MessageBoxW(
            hWnd,
            L"Unable to create a backup of the configuration file.",
            L"Error",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    struct _stat buffer;

    if (_wstat(file, &buffer) == 0) {
        int width = 0;
        int height = 0;
        int fps = 0;

        if (this->ui->getResolution(&width, &height, &fps)) {
            WindowMode mode = static_cast<WindowMode>(this->ui->getSelectedWindowMode());

            this->configuration->setConfiguration(width, height, fps, mode);
            this->maybeShowBorderlessTip(mode);
            this->applyDisplayForMode(width, height, mode);

            MessageBoxW(
                hWnd,
                L"The settings were successfully saved.",
                L"Success",
                MB_OK
            );

            this->configuration->reloadConfiguration();
            PostMessageW(this->hWnd, WM_USER_UPDATE_UI, 0, (LPARAM)this);
            this->syncOverlayFromUi(false);
        } else {
            MessageBoxW(
                hWnd,
                L"Please fill out every field.",
                L"Warning",
                MB_OK | MB_ICONWARNING
            );
        }

        if (this->ui->isReadOnlyLocked()) {
            SetFileAttributesW(file, FILE_ATTRIBUTE_READONLY);
        }
    } else {
        MessageBoxW(
            hWnd,
            L"Configuration file could not be found.",
            L"Error",
            MB_OK | MB_ICONERROR
        );
    }
}

void Window::revertSettings() {
    bool success = this->configuration->revertToBackup();

    if (!success) {
        MessageBoxW(
            hWnd,
            L"Unable to revert settings.",
            L"Error",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    if (this->display) {
        this->display->restore();
    }

    MessageBoxW(
        hWnd,
        L"The settings were reverted to the original state.",
        L"Success",
        MB_OK
    );

    this->configuration->reloadConfiguration();
    PostMessageW(this->hWnd, WM_USER_UPDATE_UI, 0, (LPARAM)this);
    this->syncOverlayFromUi(false);
}

void Window::persistCrosshair() {
    if (!this->ui || !this->crosshair) {
        return;
    }

    this->ui->syncCrosshairTo(*this->crosshair);
    this->crosshair->save();
}

void Window::syncOverlayFromUi(bool reloadImage) {
    if (!this->ui || !this->crosshair || !this->overlay) {
        return;
    }

    this->ui->syncCrosshairTo(*this->crosshair);

    if (!this->overlay->isActive()) {
        return;
    }

    int width = 0;
    int height = 0;
    if (this->ui->getOverlaySize(&width, &height)) {
        this->overlay->setSize(width, height);
    }

    if (reloadImage) {
        this->overlay->reloadImage();
    }

    this->overlay->refresh();
}

void Window::startOverlay() {
    if (!this->ui || !this->crosshair || !this->overlay) {
        return;
    }

    if (this->overlay->isActive()) {
        return;
    }

    if (this->ui->getSelectedWindowMode() == MODE_FULLSCREEN) {
        const int choice = MessageBoxW(
            this->hWnd,
            L"Crosshair overlay does not work in Exclusive Fullscreen.\n\n"
            L"Switch Resolution to Borderless and open that tab so you can Apply?",
            L"Overlay needs Borderless / Windowed",
            MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON1
        );

        if (choice == IDYES) {
            this->ui->setWindowMode(IDC_WFS_RADIO);
            this->ui->selectTab(0);
            this->maybeShowBorderlessTip(MODE_WINDOWED_FULLSCREEN);
        }
        return;
    }

    int width = 0;
    int height = 0;
    if (!this->ui->getOverlaySize(&width, &height)) {
        MessageBoxW(
            this->hWnd,
            L"Resolution must be positive integers on the Resolution tab.",
            L"Warning",
            MB_OK | MB_ICONWARNING
        );
        return;
    }

    this->ui->syncCrosshairTo(*this->crosshair);
    if (!this->overlay->create(this->hInstance, this->crosshair, width, height)) {
        MessageBoxW(
            this->hWnd,
            L"Unable to create the crosshair overlay.",
            L"Error",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    this->persistCrosshair();
}

void Window::stopOverlay() {
    if (this->overlay) {
        this->overlay->destroy();
    }
    this->persistCrosshair();
}

void Window::pickCrosshairColor() {
    if (!this->ui || !this->ui->getCustomPngPath().empty()) {
        return;
    }

    static COLORREF customColors[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = this->hWnd;
    cc.lpCustColors = customColors;
    cc.rgbResult = this->ui->getCrossColor();
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        this->ui->setCrossColor(cc.rgbResult);
        this->syncOverlayFromUi(false);
        this->persistCrosshair();
    }
}

void Window::loadCrosshairPng() {
    wchar_t filePath[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = this->hWnd;
    ofn.lpstrFilter = L"PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"png";

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    Gdiplus::Image probe(filePath);
    if (probe.GetLastStatus() != Gdiplus::Ok) {
        MessageBoxW(this->hWnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    this->ui->setCustomPngPath(filePath);
    this->syncOverlayFromUi(true);
    this->persistCrosshair();
}

Window* Window::getObject(HWND hWnd) {
    return reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
}

void Window::center(DWORD style, DWORD exStyle) {
    (void)style;
    (void)exStyle;

    RECT rc = {};
    GetWindowRect(this->hWnd, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    SetWindowPos(this->hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

bool Window::create(const wchar_t* title, int width, int height) {
    this->registerClass(L"RetracRes");

    this->hWnd = CreateWindowExW(
        WINDOW_EXSTYLE,
        L"RetracRes",
        title,
        WINDOW_STYLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        this->hInstance,
        this
    );

    return hWnd != nullptr;
}

void Window::registerClass(const wchar_t* lpszClassName) {
    this->hBrushBackground = CreateSolidBrush(Color::BACKGROUND);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = Window::WindowProc;
    wcex.hInstance = this->hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = this->hBrushBackground;
    wcex.hIcon = LoadIcon(this->hInstance, MAKEINTRESOURCE(IDR_ICON));
    wcex.hIconSm = wcex.hIcon;
    wcex.lpszClassName = lpszClassName;

    if (!RegisterClassExW(&wcex)) {
        throw std::runtime_error("Unable to register window class.");
    }
}

void Window::applyRoundedCorners() {
    const DWORD preference = DWMWCP_ROUND;
    const HRESULT hr = DwmSetWindowAttribute(
        this->hWnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &preference,
        sizeof(preference)
    );

    if (SUCCEEDED(hr)) {
        this->use_dwm_corners = true;
        const COLORREF border = Color::BORDER;
        DwmSetWindowAttribute(this->hWnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
        SetWindowRgn(this->hWnd, nullptr, TRUE);
        return;
    }

    this->use_dwm_corners = false;
    this->updateFallbackRegion();
}

void Window::updateFallbackRegion() {
    if (this->use_dwm_corners || !this->hWnd) {
        return;
    }

    RECT rc = {};
    GetWindowRect(this->hWnd, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int dia = Chrome::kCornerRadius * 2;
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, dia, dia);
    SetWindowRgn(this->hWnd, region, TRUE);
}

RECT Window::titleControlRect(int which) const {
    RECT client = {};
    GetClientRect(this->hWnd, &client);

    const int top = (Chrome::kTitleBarHeight - Chrome::kTitleBtnSize) / 2;
    const int closeLeft = client.right - 12 - Chrome::kTitleBtnSize;
    const int minLeft = closeLeft - Chrome::kTitleBtnGap - Chrome::kTitleBtnSize;

    RECT rect = {
        which == kTitleHitClose ? closeLeft : minLeft,
        top,
        (which == kTitleHitClose ? closeLeft : minLeft) + Chrome::kTitleBtnSize,
        top + Chrome::kTitleBtnSize
    };
    return rect;
}

int Window::hitTitleControls(POINT clientPt) const {
    RECT closeRc = this->titleControlRect(kTitleHitClose);
    RECT minRc = this->titleControlRect(kTitleHitMin);

    if (PtInRect(&closeRc, clientPt)) {
        return kTitleHitClose;
    }
    if (PtInRect(&minRc, clientPt)) {
        return kTitleHitMin;
    }
    return kTitleHitNone;
}

void Window::paintTitleBar(HDC hdc) {
    RECT client = {};
    GetClientRect(this->hWnd, &client);

    RECT titleRc = { 0, 0, client.right, Chrome::kTitleBarHeight };
    {
        Draw::Buffer buffer(hdc, titleRc, Color::BACKGROUND);
        Gdiplus::Graphics& g = buffer.graphics();

        Gdiplus::Pen hairline(Draw::gp(Color::BORDER_SOFT), 1.0f);
        g.DrawLine(
            &hairline,
            0.0f,
            static_cast<float>(Chrome::kTitleBarHeight) - 0.5f,
            static_cast<float>(client.right),
            static_cast<float>(Chrome::kTitleBarHeight) - 0.5f
        );

        Draw::ScopedFont titleFont(this->hFontTitle ? this->hFontTitle : this->hFont);
        Gdiplus::SolidBrush textBrush(Draw::gp(Color::TEXT));

        // Match the icon's vertical band so the label doesn't sit optically high.
        constexpr int kIconSize = 18;
        constexpr int kIconX = 16;
        const int iconY = (Chrome::kTitleBarHeight - kIconSize) / 2;
        Gdiplus::RectF titleText(
            static_cast<float>(kIconX + kIconSize + 8),
            static_cast<float>(iconY),
            240.0f,
            static_cast<float>(kIconSize)
        );
        Draw::drawLeftText(g, L"RetracRes", titleFont.get(), textBrush, titleText);

        auto paintBtn = [&](int which, bool isClose) {
            RECT rc = this->titleControlRect(which);
            Gdiplus::RectF btn(
                static_cast<float>(rc.left),
                static_cast<float>(rc.top),
                static_cast<float>(rc.right - rc.left),
                static_cast<float>(rc.bottom - rc.top)
            );

            const bool hovered = (this->title_hover == which);
            if (hovered) {
                const COLORREF fill = isClose ? RGB(0xC4, 0x2B, 0x1C) : Color::SURFACE_HOVER;
                Gdiplus::SolidBrush brush(Draw::gp(fill));
                Draw::fillRoundRect(g, brush, btn, 8.0f);
            }

            const COLORREF glyph = (hovered && isClose) ? Color::WHITE : Color::TEXT_MUTED;
            Gdiplus::Pen pen(Draw::gp(glyph), 1.6f);
            pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
            const float cx = btn.X + btn.Width * 0.5f;
            const float cy = btn.Y + btn.Height * 0.5f;

            if (isClose) {
                g.DrawLine(&pen, cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f);
                g.DrawLine(&pen, cx + 5.0f, cy - 5.0f, cx - 5.0f, cy + 5.0f);
            } else {
                g.DrawLine(&pen, cx - 5.0f, cy, cx + 5.0f, cy);
            }
        };

        paintBtn(kTitleHitMin, false);
        paintBtn(kTitleHitClose, true);
    }

    HICON icon = LoadIconW(this->hInstance, MAKEINTRESOURCEW(IDR_ICON));
    if (icon) {
        constexpr int kIconSize = 18;
        constexpr int kIconX = 16;
        const int iconY = (Chrome::kTitleBarHeight - kIconSize) / 2;
        DrawIconEx(hdc, kIconX, iconY, icon, kIconSize, kIconSize, 0, nullptr, DI_NORMAL);
    }
}

void Window::registerFrameClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(this->hInstance, kFrameClassName, &wc)) {
        return;
    }

    wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Window::FrameProc;
    wc.hInstance = this->hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kFrameClassName;

    if (!RegisterClassExW(&wc)) {
        throw std::runtime_error("Unable to register frame class.");
    }
}

void Window::createFrameOverlay() {
    this->registerFrameClass();

    RECT client = {};
    GetClientRect(this->hWnd, &client);

    this->frameOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOACTIVATE,
        kFrameClassName,
        L"",
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        client.right,
        client.bottom,
        this->hWnd,
        nullptr,
        this->hInstance,
        this
    );

    if (!this->frameOverlay) {
        return;
    }

    // Magenta is transparent; only the RoundRect ring stays visible.
    SetLayeredWindowAttributes(this->frameOverlay, kFrameColorKey, 0, LWA_COLORKEY);
    this->layoutFrameOverlay();
}

void Window::layoutFrameOverlay() {
    if (!this->frameOverlay) {
        return;
    }

    RECT client = {};
    GetClientRect(this->hWnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    // Full rectangle — no ring HRGN (that was cutting the corners).
    SetWindowRgn(this->frameOverlay, nullptr, FALSE);
    SetWindowPos(
        this->frameOverlay,
        HWND_TOP,
        0,
        0,
        width,
        height,
        SWP_NOACTIVATE
    );
    InvalidateRect(this->frameOverlay, nullptr, TRUE);
    UpdateWindow(this->frameOverlay);
}

void Window::paintFrameOverlay(HDC hdc) const {
    RECT client = {};
    GetClientRect(this->frameOverlay, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width < 8 || height < 8) {
        return;
    }

    HBRUSH keyBrush = CreateSolidBrush(kFrameColorKey);
    FillRect(hdc, &client, keyBrush);

    // Solid outer round-rect, then magenta hole = continuous 1px ring
    // (CreateRoundRectRgn RGN_DIFF leaves gaps at the corners).
    const int outerDia = Chrome::kCornerRadius * 2;
    const int innerDia = (std::max)(2, (Chrome::kCornerRadius - 1) * 2);

    HBRUSH borderBrush = CreateSolidBrush(Color::BORDER);
    HGDIOBJ oldBrush = SelectObject(hdc, borderBrush);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, 0, 0, width, height, outerDia, outerDia);

    SelectObject(hdc, keyBrush);
    RoundRect(hdc, 1, 1, width - 1, height - 1, innerDia, innerDia);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderBrush);
    DeleteObject(keyBrush);
}

LRESULT CALLBACK Window::FrameProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (uMsg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hWnd, &ps);
            if (window) {
                window->paintFrameOverlay(hdc);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        default:
            break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void Window::show(int cmdShow) {
    ShowWindow(this->hWnd, cmdShow);
    UpdateWindow(this->hWnd);
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hWnd = hWnd;
    } else {
        window = getObject(hWnd);
    }

    if (!window) {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_COMMAND:
            window->onCommand(wParam, lParam);
            return 0;

        case WM_HSCROLL: {
            HWND trackbar = reinterpret_cast<HWND>(lParam);
            if (trackbar && window->ui) {
                window->ui->onTrackbar(trackbar);
                window->syncOverlayFromUi(false);
                if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION) {
                    window->persistCrosshair();
                }
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == 1 && window->ui) {
                window->ui->onAnimTick();
            }
            return 0;

        case WM_CREATE:
            window->onCreate();
            window->applyRoundedCorners();
            window->createFrameOverlay();
            return 0;

        case WM_SIZE:
            window->updateFallbackRegion();
            if (window->ui) {
                window->ui->onHostResize();
            }
            window->layoutFrameOverlay();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hWnd, &ps);
            if (window->ui) {
                window->ui->paint(hdc);
            }
            window->paintTitleBar(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_NCHITTEST: {
            // Keep default client hit-testing; dragging handled in WM_LBUTTONDOWN.
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int hit = window->hitTitleControls(pt);
            if (hit != window->title_hover) {
                window->title_hover = hit;
                RECT titleRc = {};
                GetClientRect(hWnd, &titleRc);
                titleRc.bottom = Chrome::kTitleBarHeight;
                InvalidateRect(hWnd, &titleRc, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (window->title_hover != 0) {
                window->title_hover = 0;
                RECT titleRc = {};
                GetClientRect(hWnd, &titleRc);
                titleRc.bottom = Chrome::kTitleBarHeight;
                InvalidateRect(hWnd, &titleRc, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int hit = window->hitTitleControls(pt);
            if (hit == kTitleHitClose) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (hit == kTitleHitMin) {
                ShowWindow(hWnd, SW_MINIMIZE);
                return 0;
            }
            if (pt.y >= 0 && pt.y < Chrome::kTitleBarHeight) {
                ReleaseCapture();
                SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            return 0;
        }

        case WM_USER_UPDATE_UI:
            if (lParam && window->ui) {
                window->ui->fromConfiguration(window->configuration);
            }
            return 0;

        case WM_CTLCOLORSTATIC:
            return window->ui->onColorStatic(wParam, lParam);

        case WM_CTLCOLOREDIT:
            return window->ui->onColorEdit(wParam);

        case WM_DESTROY:
            window->onDestroy();
            return 0;

        case WM_DRAWITEM:
            return window->ui->onDraw(wParam, lParam);

        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

void Window::onCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    const int id = LOWORD(wParam);
    const int code = HIWORD(wParam);

    if (code == EN_CHANGE && (id == IDC_WIDTH_EDIT || id == IDC_HEIGHT_EDIT || id == IDC_FPS_EDIT)) {
        if (id == IDC_WIDTH_EDIT || id == IDC_HEIGHT_EDIT) {
            this->ui->syncActivePreset();
            this->syncOverlayFromUi(false);
        }
        return;
    }

    switch (id) {
        case IDC_TAB_RESOLUTION:
            this->ui->selectTab(0);
            break;

        case IDC_TAB_CROSSHAIR:
            this->ui->selectTab(1);
            break;

        case IDC_APPLY_BUTTON:
            this->applySettings();
            break;

        case IDC_REVERT_BUTTON:
            this->revertSettings();
            break;

        case IDC_PRESET_1440:
            this->ui->applyPreset(1440, 1080);
            this->syncOverlayFromUi(false);
            break;

        case IDC_PRESET_1560:
            this->ui->applyPreset(1560, 1080);
            this->syncOverlayFromUi(false);
            break;

        case IDC_PRESET_1550:
            this->ui->applyPreset(1550, 1080);
            this->syncOverlayFromUi(false);
            break;

        case IDC_FS_RADIO:
        case IDC_WFS_RADIO:
        case IDC_W_RADIO:
            this->ui->setWindowMode(LOWORD(wParam));
            break;

        case IDC_RO_CHECKBOX:
            this->ui->toggleReadOnly();
            break;

        case IDC_CROSS_PLUS:
            this->ui->setCrossType(CrossType::Plus);
            this->syncOverlayFromUi(false);
            this->persistCrosshair();
            break;

        case IDC_CROSS_X:
            this->ui->setCrossType(CrossType::X);
            this->syncOverlayFromUi(false);
            this->persistCrosshair();
            break;

        case IDC_CROSS_DOT:
            this->ui->setCrossType(CrossType::Dot);
            this->syncOverlayFromUi(false);
            this->persistCrosshair();
            break;

        case IDC_CROSS_CIRCLE:
            this->ui->setCrossType(CrossType::Circle);
            this->syncOverlayFromUi(false);
            this->persistCrosshair();
            break;

        case IDC_CROSS_COLOR:
            this->pickCrosshairColor();
            break;

        case IDC_CROSS_LOAD_PNG:
            this->loadCrosshairPng();
            break;

        case IDC_CROSS_CLEAR_PNG:
            this->ui->clearCustomPng();
            this->syncOverlayFromUi(true);
            this->persistCrosshair();
            break;

        case IDC_CROSS_START:
            this->startOverlay();
            break;

        case IDC_CROSS_STOP:
            this->stopOverlay();
            break;

        default:
            break;
    }
}

namespace {
int CALLBACK FontExistsProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    *reinterpret_cast<bool*>(lParam) = true;
    return 0;
}

bool fontAvailable(const wchar_t* faceName) {
    bool found = false;
    HDC hdc = GetDC(nullptr);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, faceName, _TRUNCATE);
    EnumFontFamiliesExW(hdc, &lf, FontExistsProc, reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, hdc);
    return found;
}

const wchar_t* pickFallbackFace() {
    static const wchar_t* candidates[] = {
        L"Bahnschrift",
        L"Candara",
        L"Trebuchet MS",
        L"Segoe UI"
    };

    for (const wchar_t* face : candidates) {
        if (fontAvailable(face)) {
            return face;
        }
    }
    return L"Segoe UI";
}
}

void Window::onCreate() {
    if (this->fonts && this->fonts->isLoaded()) {
        this->hFont = this->fonts->create(16, FW_MEDIUM);
        this->hFontSmall = this->fonts->create(14, FW_MEDIUM);
        this->hFontTitle = this->fonts->create(14, FW_SEMIBOLD);
        this->hFontButton = this->fonts->create(15, FW_MEDIUM);
    } else {
        const wchar_t* face = pickFallbackFace();
        auto makeFallback = [face](int heightPx, int weight) {
            return CreateFontW(
                -heightPx, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face
            );
        };
        this->hFont = makeFallback(16, FW_MEDIUM);
        this->hFontSmall = makeFallback(14, FW_MEDIUM);
        this->hFontTitle = makeFallback(14, FW_SEMIBOLD);
        this->hFontButton = makeFallback(15, FW_MEDIUM);
    }

    this->crosshair->load();

    this->ui = new UserInterface(this->hWnd, this->hInstance);
    this->ui->setFonts(this->hFont, this->hFontSmall, this->hFontTitle, this->hFontButton);
    this->ui->initialize();
    this->ui->fromConfiguration(this->configuration);
    this->ui->fromCrosshair(*this->crosshair);
}

void Window::onDestroy() {
    this->persistCrosshair();
    if (this->overlay) {
        this->overlay->destroy();
    }

    DeleteObject(this->hFont);
    DeleteObject(this->hFontSmall);
    DeleteObject(this->hFontTitle);
    DeleteObject(this->hFontButton);
    DeleteObject(this->hBrushBackground);
    delete this->ui;
    this->ui = nullptr;

    if (this->fonts) {
        this->fonts->unload();
    }

    PostQuitMessage(0);
}
