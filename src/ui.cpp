#include "ui.h"
#include "draw.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <uxtheme.h>

namespace {
constexpr int kMargin = 36;
constexpr int kContentWidth = 412;
constexpr int kFieldGap = 14;
constexpr int kFieldWidth = 128;
constexpr int kFieldHeight = 44;
constexpr int kInputInset = 6;
constexpr int kChipHeight = 36;
constexpr int kChipGap = 10;
constexpr int kButtonHeight = 44;
constexpr int kRadioHeight = 38;
constexpr int kCheckSize = 18;
constexpr float kRadioOuter = 18.0f;
constexpr float kRadioInner = 10.0f;
constexpr float kIconTextGap = 14.0f;
constexpr int kTabBarTop = Chrome::kTitleBarHeight + 16;
constexpr int kTabBarHeight = 40;
constexpr int kTabTrackPad = 4;
constexpr float kTabTrackRadius = 10.0f;
// Nested radius: outer − pad keeps the active pill parallel to the track.
constexpr float kTabInnerRadius = kTabTrackRadius - static_cast<float>(kTabTrackPad);
constexpr int kPageTop = Chrome::kTitleBarHeight + 16 + 40 + 12;
constexpr int kSectionGap = 30;
constexpr int kHeaderGap = 18;
constexpr int kAnimMs = 120;
constexpr UINT_PTR kAnimTimerId = 1;

struct SubclassData {
    UserInterface* ui;
    WNDPROC original;
};

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<SubclassData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_SETFOCUS:
            data->ui->setFocusedEdit(hwnd);
            break;
        case WM_KILLFOCUS:
            data->ui->setFocusedEdit(nullptr);
            break;
        case WM_CHAR:
            if (wParam == VK_RETURN || wParam == '\n' || wParam == '\r') {
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                return 0;
            }
            break;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data->original));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete data;
            return 0;
        default:
            break;
    }

    return CallWindowProcW(data->original, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK InteractiveSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<SubclassData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            data->ui->setHoveredControl(GetDlgCtrlID(hwnd));
            break;
        }
        case WM_MOUSELEAVE:
            data->ui->setHoveredControl(0);
            break;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data->original));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete data;
            return 0;
        default:
            break;
    }

    return CallWindowProcW(data->original, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TrackbarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<SubclassData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            data->ui->paintTrackbar(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data->original));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete data;
            return 0;
        default:
            break;
    }

    return CallWindowProcW(data->original, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PanelSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<SubclassData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND mainWnd = GetParent(hwnd);

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (GetDlgCtrlID(hwnd) == IDC_PAGE_RESOLUTION) {
                data->ui->paintResolutionPage(hdc);
            } else {
                data->ui->paintCrosshairPage(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_DRAWITEM:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
            return SendMessageW(mainWnd, msg, wParam, lParam);

        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data->original));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete data;
            return 0;

        default:
            break;
    }

    return CallWindowProcW(data->original, hwnd, msg, wParam, lParam);
}
}

UserInterface::UserInterface(HWND hWnd, HINSTANCE hInstance) {
    this->hWnd = hWnd;
    this->hInstance = hInstance;
}

UserInterface::~UserInterface() {
    if (this->anim_running) {
        KillTimer(this->hWnd, kAnimTimerId);
    }
    DeleteObject(this->hBrushStatic);
    DeleteObject(this->hBrushEdit);
}

void UserInterface::initialize() {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    this->createBrush();
    this->createControl();
    this->fitWindowToContent();
}

void UserInterface::createBrush() {
    this->hBrushStatic = CreateSolidBrush(Color::BACKGROUND);
    this->hBrushEdit = CreateSolidBrush(Color::EDIT);
}

void UserInterface::createControl() {
    this->createTabs();
    this->createPages();
    this->layoutPages();
    this->createResolutionControls();
    this->createCrosshairControls();
    this->selectTab(0);
}

void UserInterface::createTabs() {
    const int tabW = (kContentWidth - kTabTrackPad * 2) / 2;
    const int tabH = kTabBarHeight - kTabTrackPad * 2;
    const int tabY = kTabBarTop + kTabTrackPad;

    this->tab_resolution = CreateWindowExW(
        0, L"Button", L"Resolution",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP | WS_GROUP,
        kMargin + kTabTrackPad, tabY, tabW, tabH,
        this->hWnd, (HMENU)IDC_TAB_RESOLUTION, this->hInstance, 0
    );
    this->subclassInteractive(this->tab_resolution);

    this->tab_crosshair = CreateWindowExW(
        0, L"Button", L"Crosshair",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + kTabTrackPad + tabW, tabY, tabW, tabH,
        this->hWnd, (HMENU)IDC_TAB_CROSSHAIR, this->hInstance, 0
    );
    this->subclassInteractive(this->tab_crosshair);
}

void UserInterface::createPages() {
    this->page_resolution = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | WS_VISIBLE | SS_NOTIFY | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        this->hWnd, (HMENU)IDC_PAGE_RESOLUTION, this->hInstance, nullptr
    );
    this->subclassPanel(this->page_resolution);

    this->page_crosshair = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_NOTIFY | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        this->hWnd, (HMENU)IDC_PAGE_CROSSHAIR, this->hInstance, nullptr
    );
    this->subclassPanel(this->page_crosshair);
}

void UserInterface::layoutPages() {
    RECT client = {};
    GetClientRect(this->hWnd, &client);

    const int inset = Chrome::kFrameInset;
    const int y = kPageTop;
    const int w = client.right - client.left - inset * 2;
    const int h = client.bottom - y - inset;

    // Keep existing z-order — never raise pages above the frame overlay,
    // or the page rect will cover the rounded border corners.
    SetWindowPos(this->page_resolution, nullptr, inset, y, w, h, SWP_NOZORDER);
    SetWindowPos(this->page_crosshair, nullptr, inset, y, w, h, SWP_NOZORDER);
}

void UserInterface::onHostResize() {
    this->layoutPages();
}

void UserInterface::fitWindowToContent() {
    if (!this->hWnd) {
        return;
    }

    auto bottomOf = [this](HWND control) -> int {
        if (!control) {
            return 0;
        }
        RECT rc = {};
        GetWindowRect(control, &rc);
        MapWindowPoints(HWND_DESKTOP, this->hWnd, reinterpret_cast<LPPOINT>(&rc), 2);
        return rc.bottom;
    };

    const int bottom = (std::max)({
        bottomOf(this->apply_button),
        bottomOf(this->revert_button),
        bottomOf(this->cross_start),
        bottomOf(this->cross_stop)
    });

    if (bottom <= 0) {
        return;
    }

    const int desired = bottom + Chrome::kFrameInset + 16;
    RECT client = {};
    GetClientRect(this->hWnd, &client);
    if (client.bottom == desired) {
        return;
    }
    const int delta = client.bottom > desired ? client.bottom - desired : desired - client.bottom;
    if (delta <= 1) {
        return;
    }

    RECT windowRc = {};
    GetWindowRect(this->hWnd, &windowRc);
    const int width = windowRc.right - windowRc.left;
    SetWindowPos(
        this->hWnd,
        nullptr,
        0,
        0,
        width,
        desired,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
    );
    // WM_SIZE already runs layoutPages + raises the frame overlay.
}

void UserInterface::createResolutionControls() {
    this->createLabel();
    this->createEdit();
    this->createPresets();
    this->createRadio();
    this->createCheckbox();
    this->createApplyButton();
    this->createRevertButton();
    this->updateModeHint();
}

void UserInterface::createCrosshairControls() {
    HWND parent = this->page_crosshair;
    int y = 14;

    this->cross_type_label = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin + 24, y, kContentWidth - 24, 16,
        parent, (HMENU)IDC_CROSS_TYPE_LABEL, this->hInstance, 0
    );
    y += 16 + kHeaderGap;

    // Same nested track/pill geometry as Resolution / Crosshair tabs.
    const int trackH = kTabBarHeight;
    const int pad = kTabTrackPad;
    const int segH = trackH - pad * 2;
    const int innerW = kContentWidth - pad * 2;
    const int segW = innerW / 4;
    const int btnY = y + pad;
    const int btnX = kMargin + pad;

    this->cross_plus = CreateWindowExW(
        0, L"Button", L"+",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP | WS_GROUP,
        btnX, btnY, segW, segH,
        parent, (HMENU)IDC_CROSS_PLUS, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_plus);

    this->cross_x = CreateWindowExW(
        0, L"Button", L"x",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        btnX + segW, btnY, segW, segH,
        parent, (HMENU)IDC_CROSS_X, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_x);

    this->cross_dot = CreateWindowExW(
        0, L"Button", L"Dot",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        btnX + 2 * segW, btnY, segW, segH,
        parent, (HMENU)IDC_CROSS_DOT, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_dot);

    this->cross_circle = CreateWindowExW(
        0, L"Button", L"Circle",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        btnX + 3 * segW, btnY, innerW - 3 * segW, segH,
        parent, (HMENU)IDC_CROSS_CIRCLE, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_circle);
    y += trackH + 20;

    const int halfBtn = (kContentWidth - kChipGap) / 2;
    this->cross_load_png = CreateWindowExW(
        0, L"Button", L"Load PNG",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, y, halfBtn, kButtonHeight,
        parent, (HMENU)IDC_CROSS_LOAD_PNG, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_load_png);

    this->cross_clear_png = CreateWindowExW(
        0, L"Button", L"Clear PNG",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + halfBtn + kChipGap, y, halfBtn, kButtonHeight,
        parent, (HMENU)IDC_CROSS_CLEAR_PNG, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_clear_png);
    y += kButtonHeight + kSectionGap;

    this->cross_scale_label = CreateWindowExW(
        0, L"Static", L"Custom size (PNG)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, y, kContentWidth, 16,
        parent, (HMENU)IDC_CROSS_SCALE_LABEL, this->hInstance, 0
    );
    SendMessageW(this->cross_scale_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);
    y += 16 + 10;

    this->cross_scale = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_NOTICKS | WS_TABSTOP,
        kMargin, y, kContentWidth, 28,
        parent, (HMENU)IDC_CROSS_SCALE, this->hInstance, 0
    );
    SendMessageW(this->cross_scale, TBM_SETRANGE, TRUE, MAKELONG(10, 300));
    SendMessageW(this->cross_scale, TBM_SETPOS, TRUE, 100);
    this->styleTrackbar(this->cross_scale);
    EnableWindow(this->cross_scale, FALSE);
    y += 28 + 20;

    this->cross_color_label = CreateWindowExW(
        0, L"Static", L"Color",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, y + 8, 90, 16,
        parent, (HMENU)IDC_CROSS_COLOR_LABEL, this->hInstance, 0
    );
    SendMessageW(this->cross_color_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);

    this->cross_color_button = CreateWindowExW(
        0, L"Button", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + 100, y, 42, 32,
        parent, (HMENU)IDC_CROSS_COLOR, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_color_button);
    y += 32 + 20;

    this->cross_thick_label = CreateWindowExW(
        0, L"Static", L"Line thickness",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, y, kContentWidth, 16,
        parent, (HMENU)IDC_CROSS_THICK_LABEL, this->hInstance, 0
    );
    SendMessageW(this->cross_thick_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);
    y += 16 + 10;

    this->cross_thick = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_NOTICKS | WS_TABSTOP,
        kMargin, y, kContentWidth, 28,
        parent, (HMENU)IDC_CROSS_THICK, this->hInstance, 0
    );
    SendMessageW(this->cross_thick, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
    SendMessageW(this->cross_thick, TBM_SETPOS, TRUE, 2);
    this->styleTrackbar(this->cross_thick);
    y += 28 + 20;

    this->cross_length_label = CreateWindowExW(
        0, L"Static", L"Line length",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, y, kContentWidth, 16,
        parent, (HMENU)IDC_CROSS_LENGTH_LABEL, this->hInstance, 0
    );
    SendMessageW(this->cross_length_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);
    y += 16 + 10;

    this->cross_length = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_NOTICKS | WS_TABSTOP,
        kMargin, y, kContentWidth, 28,
        parent, (HMENU)IDC_CROSS_LENGTH, this->hInstance, 0
    );
    SendMessageW(this->cross_length, TBM_SETRANGE, TRUE, MAKELONG(5, 100));
    SendMessageW(this->cross_length, TBM_SETPOS, TRUE, 20);
    this->styleTrackbar(this->cross_length);
    y += 28 + kSectionGap;

    this->cross_hint = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin, y, kContentWidth, 40,
        parent, (HMENU)IDC_CROSS_HINT, this->hInstance, 0
    );
    ShowWindow(this->cross_hint, SW_HIDE);

    this->cross_start = CreateWindowExW(
        0, L"Button", L"Start",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, 0, halfBtn, kButtonHeight,
        parent, (HMENU)IDC_CROSS_START, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_start);

    this->cross_stop = CreateWindowExW(
        0, L"Button", L"Stop",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + halfBtn + kChipGap, 0, halfBtn, kButtonHeight,
        parent, (HMENU)IDC_CROSS_STOP, this->hInstance, 0
    );
    this->subclassInteractive(this->cross_stop);

    this->updateCrosshairHint();
}

void UserInterface::styleTrackbar(HWND trackbar) {
    if (!trackbar) {
        return;
    }
    SetWindowTheme(trackbar, L"", L"");
    this->subclassTrackbar(trackbar);
}

void UserInterface::setFonts(HFONT body, HFONT small, HFONT title, HFONT button) {
    this->hFont = body;
    this->hFontSmall = small;
    this->hFontTitle = title;
    this->hFontButton = button;
}

void UserInterface::subclassEdit(HWND edit) {
    auto* data = new SubclassData();
    data->ui = this;
    data->original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc))
    );
    SetWindowLongPtrW(edit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
}

void UserInterface::subclassInteractive(HWND control) {
    auto* data = new SubclassData();
    data->ui = this;
    data->original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(control, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(InteractiveSubclassProc))
    );
    SetWindowLongPtrW(control, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
}

void UserInterface::subclassPanel(HWND panel) {
    auto* data = new SubclassData();
    data->ui = this;
    data->original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(panel, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PanelSubclassProc))
    );
    SetWindowLongPtrW(panel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
}

void UserInterface::subclassTrackbar(HWND trackbar) {
    auto* data = new SubclassData();
    data->ui = this;
    data->original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(trackbar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TrackbarSubclassProc))
    );
    SetWindowLongPtrW(trackbar, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
}

void UserInterface::centerEditText(HWND edit) {
    RECT client = {};
    GetClientRect(edit, &client);

    HDC hdc = GetDC(edit);
    HGDIOBJ old = SelectObject(hdc, this->hFont);
    TEXTMETRICW tm = {};
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, old);
    ReleaseDC(edit, hdc);

    const int padX = 10;
    const int textH = tm.tmHeight;
    const int top = (std::max)(0, static_cast<int>((client.bottom - client.top - textH) / 2));

    RECT format = { padX, top, client.right - padX, top + textH };
    SendMessageW(edit, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&format));
}

void UserInterface::createLabel() {
    HWND parent = this->page_resolution;
    int y = 14;

    this->display_label = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin + 24, y, kContentWidth - 24, 16,
        parent, (HMENU)IDC_DISPLAY_LABEL, this->hInstance, 0
    );
    y += 16 + kHeaderGap;

    this->width_label = CreateWindowExW(
        0, L"Static", L"Width",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, y, kFieldWidth, 16,
        parent, (HMENU)IDC_WIDTH_LABEL, this->hInstance, 0
    );
    SendMessageW(this->width_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);

    this->height_label = CreateWindowExW(
        0, L"Static", L"Height",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin + kFieldWidth + kFieldGap, y, kFieldWidth, 16,
        parent, (HMENU)IDC_HEIGHT_LABEL, this->hInstance, 0
    );
    SendMessageW(this->height_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);

    this->fps_label = CreateWindowExW(
        0, L"Static", L"FPS",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin + 2 * (kFieldWidth + kFieldGap), y, kFieldWidth, 16,
        parent, (HMENU)IDC_FPS_LABEL, this->hInstance, 0
    );
    SendMessageW(this->fps_label, WM_SETFONT, (WPARAM)this->hFontSmall, TRUE);

    this->presets_label = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin + 24, 148, kContentWidth - 24, 16,
        parent, (HMENU)IDC_PRESETS_LABEL, this->hInstance, 0
    );

    this->mode_label = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin + 24, 248, kContentWidth - 24, 16,
        parent, (HMENU)IDC_MODE_LABEL, this->hInstance, 0
    );
}

void UserInterface::createEdit() {
    HWND parent = this->page_resolution;
    // labels at 14+16+18=48 → edits at 48+16+10=74
    const int y = 74;
    const DWORD style = WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | ES_MULTILINE | ES_AUTOHSCROLL | WS_TABSTOP;
    const int editW = kFieldWidth - 2 * kInputInset;
    const int editH = kFieldHeight - 2 * kInputInset;

    this->width_edit = CreateWindowExW(
        0, L"Edit", L"1920", style,
        kMargin + kInputInset, y + kInputInset, editW, editH,
        parent, (HMENU)IDC_WIDTH_EDIT, this->hInstance, 0
    );
    SendMessageW(this->width_edit, WM_SETFONT, (WPARAM)this->hFont, TRUE);
    SendMessageW(this->width_edit, EM_SETLIMITTEXT, 4, 0);
    this->subclassEdit(this->width_edit);
    this->centerEditText(this->width_edit);

    this->height_edit = CreateWindowExW(
        0, L"Edit", L"1080", style,
        kMargin + kFieldWidth + kFieldGap + kInputInset, y + kInputInset, editW, editH,
        parent, (HMENU)IDC_HEIGHT_EDIT, this->hInstance, 0
    );
    SendMessageW(this->height_edit, WM_SETFONT, (WPARAM)this->hFont, TRUE);
    SendMessageW(this->height_edit, EM_SETLIMITTEXT, 4, 0);
    this->subclassEdit(this->height_edit);
    this->centerEditText(this->height_edit);

    this->fps_edit = CreateWindowExW(
        0, L"Edit", L"0", style,
        kMargin + 2 * (kFieldWidth + kFieldGap) + kInputInset, y + kInputInset, editW, editH,
        parent, (HMENU)IDC_FPS_EDIT, this->hInstance, 0
    );
    SendMessageW(this->fps_edit, WM_SETFONT, (WPARAM)this->hFont, TRUE);
    SendMessageW(this->fps_edit, EM_SETLIMITTEXT, 3, 0);
    this->subclassEdit(this->fps_edit);
    this->centerEditText(this->fps_edit);
}

void UserInterface::createPresets() {
    HWND parent = this->page_resolution;
    // edits end 74+44=118 + 30 = 148 header, +16+18 = 182 chips
    const int headerY = 148;
    const int y = headerY + 16 + kHeaderGap;
    const int chipW = (kContentWidth - 2 * kChipGap) / 3;

    SetWindowPos(this->presets_label, nullptr, kMargin + 24, headerY, kContentWidth - 24, 16, SWP_NOZORDER);

    this->preset_1440 = CreateWindowExW(
        0, L"Button", L"1440x1080",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, y, chipW, kChipHeight,
        parent, (HMENU)IDC_PRESET_1440, this->hInstance, 0
    );
    this->subclassInteractive(this->preset_1440);

    this->preset_1560 = CreateWindowExW(
        0, L"Button", L"1560x1080",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + chipW + kChipGap, y, chipW, kChipHeight,
        parent, (HMENU)IDC_PRESET_1560, this->hInstance, 0
    );
    this->subclassInteractive(this->preset_1560);

    this->preset_1550 = CreateWindowExW(
        0, L"Button", L"1550x1080",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + 2 * (chipW + kChipGap), y, chipW, kChipHeight,
        parent, (HMENU)IDC_PRESET_1550, this->hInstance, 0
    );
    this->subclassInteractive(this->preset_1550);
}

void UserInterface::createRadio() {
    HWND parent = this->page_resolution;
    // chips end 182+36=218 + 30 = 248 header
    const int headerY = 248;
    int y = headerY + 16 + kHeaderGap;

    SetWindowPos(this->mode_label, nullptr, kMargin + 24, headerY, kContentWidth - 24, 16, SWP_NOZORDER);

    this->fullscreen_radio = CreateWindowExW(
        0, L"Button", L"Fullscreen",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP | WS_GROUP,
        kMargin, y, kContentWidth, kRadioHeight,
        parent, (HMENU)IDC_FS_RADIO, this->hInstance, 0
    );
    this->subclassInteractive(this->fullscreen_radio);
    y += kRadioHeight;

    this->windowed_fullscreen_radio = CreateWindowExW(
        0, L"Button", L"Borderless",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, y, kContentWidth, kRadioHeight,
        parent, (HMENU)IDC_WFS_RADIO, this->hInstance, 0
    );
    this->subclassInteractive(this->windowed_fullscreen_radio);
    y += kRadioHeight;

    this->windowed_radio = CreateWindowExW(
        0, L"Button", L"Windowed",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, y, kContentWidth, kRadioHeight,
        parent, (HMENU)IDC_W_RADIO, this->hInstance, 0
    );
    this->subclassInteractive(this->windowed_radio);
    y += kRadioHeight + 14;

    this->mode_hint = CreateWindowExW(
        0, L"Static", L"",
        WS_CHILD | SS_LEFT,
        kMargin, y, kContentWidth, 40,
        parent, (HMENU)IDC_MODE_HINT, this->hInstance, 0
    );
    ShowWindow(this->mode_hint, SW_HIDE);
    // Text + responsive height applied after footer controls exist.
}

void UserInterface::createCheckbox() {
    this->readonly_checkbox = CreateWindowExW(
        0, L"Button", L"Lock settings file",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, 0, kContentWidth, 32,
        this->page_resolution, (HMENU)IDC_RO_CHECKBOX, this->hInstance, 0
    );
    this->read_only_locked = true;
    this->subclassInteractive(this->readonly_checkbox);
}

void UserInterface::createApplyButton() {
    this->apply_button = CreateWindowExW(
        0, L"Button", L"Apply",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin, 0, 270, kButtonHeight,
        this->page_resolution, (HMENU)IDC_APPLY_BUTTON, this->hInstance, 0
    );
    this->subclassInteractive(this->apply_button);
}

void UserInterface::createRevertButton() {
    this->revert_button = CreateWindowExW(
        0, L"Button", L"Revert",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        kMargin + 282, 0, 130, kButtonHeight,
        this->page_resolution, (HMENU)IDC_REVERT_BUTTON, this->hInstance, 0
    );
    this->subclassInteractive(this->revert_button);
}

void UserInterface::selectTab(int index) {
    this->selected_tab = index;
    this->onTabChanged();
}

void UserInterface::onTabChanged() {
    const bool showResolution = this->selected_tab == 0;

    ShowWindow(this->page_resolution, showResolution ? SW_SHOW : SW_HIDE);
    ShowWindow(this->page_crosshair, showResolution ? SW_HIDE : SW_SHOW);

    if (showResolution) {
        InvalidateRect(this->page_resolution, nullptr, FALSE);
    } else {
        InvalidateRect(this->page_crosshair, nullptr, FALSE);
    }

    this->invalidateTabs();
    InvalidateRect(this->hWnd, nullptr, FALSE);
}

void UserInterface::setFocusedEdit(HWND edit) {
    this->focused_edit = edit;
    if (this->page_resolution) {
        InvalidateRect(this->page_resolution, nullptr, FALSE);
    }
}

void UserInterface::startHoverAnim() {
    if (!this->anim_running) {
        SetTimer(this->hWnd, kAnimTimerId, 16, nullptr);
        this->anim_running = true;
    }
}

void UserInterface::setHoveredControl(int controlId) {
    if (this->hovered_control == controlId) {
        return;
    }

    if (this->hovered_control != 0) {
        this->fading_control = this->hovered_control;
        this->fade_t = this->hover_t;
    }

    this->hovered_control = controlId;
    this->hover_t = 0.0f;
    this->startHoverAnim();
    this->invalidateInteractive();
}

void UserInterface::onAnimTick() {
    const float step = 16.0f / static_cast<float>(kAnimMs);
    bool changed = false;
    bool busy = false;

    if (this->hovered_control != 0 && this->hover_t < 1.0f) {
        this->hover_t = (std::min)(1.0f, this->hover_t + step);
        changed = true;
        busy = this->hover_t < 1.0f;
    }

    if (this->fading_control != 0 && this->fade_t > 0.0f) {
        this->fade_t = (std::max)(0.0f, this->fade_t - step);
        changed = true;
        busy = busy || this->fade_t > 0.0f;
        if (this->fade_t <= 0.0f) {
            this->fading_control = 0;
        }
    }

    if (changed) {
        this->invalidateInteractive();
    }

    // Stop once settled — even while still hovered (was causing Crosshair flicker).
    if (!busy) {
        KillTimer(this->hWnd, kAnimTimerId);
        this->anim_running = false;
    }
}

float UserInterface::hoverAmount(int controlId) const {
    if (controlId == this->hovered_control) {
        return this->hover_t;
    }
    if (controlId == this->fading_control) {
        return this->fade_t;
    }
    return 0.0f;
}

float UserInterface::pressAmount(LPDRAWITEMSTRUCT pdis) const {
    return (pdis->itemState & ODS_SELECTED) ? 1.0f : 0.0f;
}

void UserInterface::invalidateInteractive() {
    this->invalidateTabs();
    this->invalidateControl(this->preset_1440);
    this->invalidateControl(this->preset_1560);
    this->invalidateControl(this->preset_1550);
    this->invalidateControl(this->fullscreen_radio);
    this->invalidateControl(this->windowed_fullscreen_radio);
    this->invalidateControl(this->windowed_radio);
    this->invalidateControl(this->readonly_checkbox);
    this->invalidateControl(this->apply_button);
    this->invalidateControl(this->revert_button);
    this->invalidateCrossTypeControls();
    this->invalidateControl(this->cross_load_png);
    this->invalidateControl(this->cross_clear_png);
    this->invalidateControl(this->cross_color_button);
    this->invalidateControl(this->cross_start);
    this->invalidateControl(this->cross_stop);
}

void UserInterface::invalidateControl(HWND control) {
    if (control) {
        InvalidateRect(control, nullptr, FALSE);
    }
}

void UserInterface::invalidateCrossTypeControls() {
    this->invalidateControl(this->cross_plus);
    this->invalidateControl(this->cross_x);
    this->invalidateControl(this->cross_dot);
    this->invalidateControl(this->cross_circle);
}

void UserInterface::invalidateTabs() {
    this->invalidateControl(this->tab_resolution);
    this->invalidateControl(this->tab_crosshair);
}

void UserInterface::applyPreset(int width, int height) {
    SetWindowTextW(this->width_edit, std::to_wstring(width).c_str());
    SetWindowTextW(this->height_edit, std::to_wstring(height).c_str());
    this->centerEditText(this->width_edit);
    this->centerEditText(this->height_edit);
    this->active_preset_width = width;
    this->active_preset_height = height;

    this->invalidateControl(this->preset_1440);
    this->invalidateControl(this->preset_1560);
    this->invalidateControl(this->preset_1550);
}

void UserInterface::setWindowMode(int controlId) {
    this->selected_mode_id = controlId;

    this->invalidateControl(this->fullscreen_radio);
    this->invalidateControl(this->windowed_fullscreen_radio);
    this->invalidateControl(this->windowed_radio);
    this->updateModeHint();
    this->updateCrosshairHint();
}

void UserInterface::updateModeHint() {
    switch (this->selected_mode_id) {
        case IDC_WFS_RADIO:
        case IDC_W_RADIO:
            this->mode_hint_text =
                L"RetracRes switches Windows to Width x Height. Create that "
                L"resolution in NVIDIA/AMD first (GPU scaling: Full screen). "
                L"Required for the crosshair overlay.";
            break;
        case IDC_FS_RADIO:
        default:
            this->mode_hint_text =
                L"Lowest latency. Fortnite applies the custom resolution. "
                L"Crosshair overlay will not appear in Exclusive Fullscreen.";
            break;
    }

    this->layoutModeHint();
    this->fitWindowToContent();

    if (this->page_resolution) {
        InvalidateRect(this->page_resolution, nullptr, FALSE);
    }
}

void UserInterface::updateCrosshairHint() {
    this->cross_hint_text =
        this->selected_mode_id == IDC_FS_RADIO
            ? L"Overlay does not work in Exclusive Fullscreen. Switch to "
              L"Borderless or Windowed on the Resolution tab, then Apply."
            : L"Overlay size uses Width x Height from Resolution. Works in "
              L"Borderless / Windowed only.";

    this->layoutCrosshairHint();
    this->fitWindowToContent();

    if (this->page_crosshair) {
        InvalidateRect(this->page_crosshair, nullptr, FALSE);
    }
}

int UserInterface::measureHintHeight(const std::wstring& text) const {
    if (!this->hFontSmall) {
        return 48;
    }

    HDC hdc = GetDC(this->hWnd ? this->hWnd : nullptr);
    if (!hdc) {
        return 48;
    }

    Gdiplus::Graphics g(hdc);
    Draw::configure(g);
    Draw::ScopedFont font(this->hFontSmall);
    const int height = Draw::measureTipHeight(
        g,
        text.c_str(),
        font.get(),
        static_cast<float>(kContentWidth)
    );
    ReleaseDC(this->hWnd, hdc);
    return height;
}

void UserInterface::layoutModeHint() {
    if (!this->mode_hint) {
        return;
    }

    RECT rc = {};
    GetWindowRect(this->mode_hint, &rc);
    MapWindowPoints(HWND_DESKTOP, this->page_resolution, reinterpret_cast<LPPOINT>(&rc), 2);

    const int top = rc.top;
    const int height = this->measureHintHeight(this->mode_hint_text);
    SetWindowPos(
        this->mode_hint,
        nullptr,
        kMargin,
        top,
        kContentWidth,
        height,
        SWP_NOZORDER
    );

    if (!this->readonly_checkbox || !this->apply_button || !this->revert_button) {
        return;
    }

    int y = top + height + 18;
    SetWindowPos(
        this->readonly_checkbox,
        nullptr,
        kMargin,
        y,
        kContentWidth,
        32,
        SWP_NOZORDER
    );
    y += 32 + 18;
    SetWindowPos(this->apply_button, nullptr, kMargin, y, 270, kButtonHeight, SWP_NOZORDER);
    SetWindowPos(this->revert_button, nullptr, kMargin + 282, y, 130, kButtonHeight, SWP_NOZORDER);
}

void UserInterface::layoutCrosshairHint() {
    if (!this->cross_hint) {
        return;
    }

    RECT rc = {};
    GetWindowRect(this->cross_hint, &rc);
    MapWindowPoints(HWND_DESKTOP, this->page_crosshair, reinterpret_cast<LPPOINT>(&rc), 2);

    const int top = rc.top;
    const int height = this->measureHintHeight(this->cross_hint_text);
    SetWindowPos(
        this->cross_hint,
        nullptr,
        kMargin,
        top,
        kContentWidth,
        height,
        SWP_NOZORDER
    );

    if (!this->cross_start || !this->cross_stop) {
        return;
    }

    const int halfBtn = (kContentWidth - kChipGap) / 2;
    const int y = top + height + 18;
    SetWindowPos(this->cross_start, nullptr, kMargin, y, halfBtn, kButtonHeight, SWP_NOZORDER);
    SetWindowPos(
        this->cross_stop,
        nullptr,
        kMargin + halfBtn + kChipGap,
        y,
        halfBtn,
        kButtonHeight,
        SWP_NOZORDER
    );
}

void UserInterface::toggleReadOnly() {
    this->read_only_locked = !this->read_only_locked;
    this->invalidateControl(this->readonly_checkbox);
}

int UserInterface::getSelectedWindowMode() const {
    if (this->selected_mode_id == IDC_FS_RADIO) {
        return MODE_FULLSCREEN;
    }
    if (this->selected_mode_id == IDC_WFS_RADIO) {
        return MODE_WINDOWED_FULLSCREEN;
    }
    return MODE_WINDOWED;
}

bool UserInterface::isReadOnlyLocked() const {
    return this->read_only_locked;
}

bool UserInterface::getResolution(int* width, int* height, int* fps) const {
    if (!width || !height || !fps) {
        return false;
    }

    wchar_t buffer[16] = {};
    GetWindowTextW(this->width_edit, buffer, 16);
    *width = _wtoi(buffer);
    GetWindowTextW(this->height_edit, buffer, 16);
    *height = _wtoi(buffer);
    GetWindowTextW(this->fps_edit, buffer, 16);
    *fps = _wtoi(buffer);

    return GetWindowTextLengthW(this->width_edit) > 0
        && GetWindowTextLengthW(this->height_edit) > 0
        && GetWindowTextLengthW(this->fps_edit) > 0;
}

bool UserInterface::getOverlaySize(int* width, int* height) const {
    if (!width || !height) {
        return false;
    }

    wchar_t buffer[16] = {};
    GetWindowTextW(this->width_edit, buffer, 16);
    *width = _wtoi(buffer);
    GetWindowTextW(this->height_edit, buffer, 16);
    *height = _wtoi(buffer);

    return *width > 0 && *height > 0;
}

void UserInterface::syncActivePreset() {
    int width = 0;
    int height = 0;
    int fps = 0;
    if (this->getResolution(&width, &height, &fps)) {
        this->active_preset_width = width;
        this->active_preset_height = height;
    } else {
        this->active_preset_width = 0;
        this->active_preset_height = 0;
    }

    this->invalidateControl(this->preset_1440);
    this->invalidateControl(this->preset_1560);
    this->invalidateControl(this->preset_1550);
}

void UserInterface::fromConfiguration(Configuration* configuration) {
    SetWindowTextW(this->width_edit, configuration->getWidth().c_str());
    SetWindowTextW(this->height_edit, configuration->getHeight().c_str());
    SetWindowTextW(this->fps_edit, configuration->getFrameRate().c_str());
    this->centerEditText(this->width_edit);
    this->centerEditText(this->height_edit);
    this->centerEditText(this->fps_edit);

    const std::wstring window_mode = configuration->getWindowMode();
    if (window_mode == L"0") {
        this->setWindowMode(IDC_FS_RADIO);
    } else if (window_mode == L"1") {
        this->setWindowMode(IDC_WFS_RADIO);
    } else {
        this->setWindowMode(IDC_W_RADIO);
    }

    std::wstring path = configuration->getPath();
    DWORD attributes = GetFileAttributesW(path.c_str());

    this->read_only_locked =
        attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
    this->invalidateControl(this->readonly_checkbox);
    this->syncActivePreset();
}

void UserInterface::fromCrosshair(const CrosshairSettings& settings) {
    this->cross_type = settings.type;
    this->cross_color_value = settings.color;
    this->custom_png_path = settings.customPngPath;

    SendMessageW(this->cross_thick, TBM_SETPOS, TRUE, settings.thickness);
    SendMessageW(this->cross_length, TBM_SETPOS, TRUE, settings.length);
    SendMessageW(this->cross_scale, TBM_SETPOS, TRUE, settings.customScale);

    this->updateStandardControlsEnabled();
    this->invalidateCrossTypeControls();
    this->invalidateControl(this->cross_color_button);
}

void UserInterface::syncCrosshairTo(CrosshairSettings& settings) const {
    settings.type = this->cross_type;
    settings.color = this->cross_color_value;
    settings.thickness = this->getCrossThickness();
    settings.length = this->getCrossLength();
    settings.customScale = this->getCustomScale();
    settings.customPngPath = this->custom_png_path;
}

void UserInterface::setCrossType(CrossType type) {
    if (!this->custom_png_path.empty()) {
        return;
    }

    this->cross_type = type;
    this->invalidateCrossTypeControls();
}

void UserInterface::setCrossColor(COLORREF color) {
    if (!this->custom_png_path.empty()) {
        return;
    }

    this->cross_color_value = color;
    this->invalidateControl(this->cross_color_button);
}

void UserInterface::onTrackbar(HWND trackbar) {
    if (trackbar) {
        InvalidateRect(trackbar, nullptr, FALSE);
    }
}

void UserInterface::setCustomPngPath(const std::wstring& path) {
    this->custom_png_path = path;
    this->updateStandardControlsEnabled();
}

void UserInterface::clearCustomPng() {
    this->custom_png_path.clear();
    this->updateStandardControlsEnabled();
}

void UserInterface::updateStandardControlsEnabled() {
    const bool standard = this->custom_png_path.empty();

    EnableWindow(this->cross_plus, standard);
    EnableWindow(this->cross_x, standard);
    EnableWindow(this->cross_dot, standard);
    EnableWindow(this->cross_circle, standard);
    EnableWindow(this->cross_color_button, standard);
    EnableWindow(this->cross_thick, standard);
    EnableWindow(this->cross_length, standard);
    EnableWindow(this->cross_scale, !standard);

    this->invalidateCrossTypeControls();
    this->invalidateControl(this->cross_color_button);
    this->invalidateControl(this->cross_thick);
    this->invalidateControl(this->cross_length);
    this->invalidateControl(this->cross_scale);
}

CrossType UserInterface::getCrossType() const {
    return this->cross_type;
}

COLORREF UserInterface::getCrossColor() const {
    return this->cross_color_value;
}

int UserInterface::getCrossThickness() const {
    return static_cast<int>(SendMessageW(this->cross_thick, TBM_GETPOS, 0, 0));
}

int UserInterface::getCrossLength() const {
    return static_cast<int>(SendMessageW(this->cross_length, TBM_GETPOS, 0, 0));
}

int UserInterface::getCustomScale() const {
    return static_cast<int>(SendMessageW(this->cross_scale, TBM_GETPOS, 0, 0));
}

const std::wstring& UserInterface::getCustomPngPath() const {
    return this->custom_png_path;
}

void UserInterface::paintTrackbar(HWND trackbar, HDC hdc) {
    RECT rc = {};
    GetClientRect(trackbar, &rc);

    Draw::Buffer buffer(hdc, rc);
    Gdiplus::Graphics& g = buffer.graphics();

    const int pos = static_cast<int>(SendMessageW(trackbar, TBM_GETPOS, 0, 0));
    const int minV = static_cast<int>(SendMessageW(trackbar, TBM_GETRANGEMIN, 0, 0));
    const int maxV = static_cast<int>(SendMessageW(trackbar, TBM_GETRANGEMAX, 0, 0));
    const float span = static_cast<float>((std::max)(1, maxV - minV));
    const float value01 = static_cast<float>(pos - minV) / span;
    const bool enabled = IsWindowEnabled(trackbar) != FALSE;

    Draw::drawSlider(g, buffer.localRect(), value01, enabled);
}

void UserInterface::drawInputChrome(HDC hdc, HWND parent, HWND edit) {
    RECT editRc = {};
    GetWindowRect(edit, &editRc);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&editRc), 2);

    RECT chrome = editRc;
    InflateRect(&chrome, kInputInset, kInputInset);

    Gdiplus::Graphics g(hdc);
    Draw::configure(g);

    Gdiplus::RectF rect(
        static_cast<float>(chrome.left) + 0.5f,
        static_cast<float>(chrome.top) + 0.5f,
        static_cast<float>(chrome.right - chrome.left) - 1.0f,
        static_cast<float>(chrome.bottom - chrome.top) - 1.0f
    );

    const bool focused = (edit == this->focused_edit);

    Gdiplus::SolidBrush fill(Draw::gp(Color::EDIT));
    Draw::fillRoundRect(g, fill, rect, Draw::kRadiusSm);

    Gdiplus::Pen border(Draw::gp(focused ? Color::ACCENT : Color::BORDER), Draw::kBorder);
    Draw::drawRoundRect(g, border, rect, Draw::kRadiusSm);
}

void UserInterface::paint(HDC hdc) {
    RECT client = {};
    GetClientRect(this->hWnd, &client);

    Gdiplus::Graphics g(hdc);
    Draw::configure(g);

    // Leave the custom title bar area alone — Window paints it.
    Gdiplus::SolidBrush bg(Draw::gp(Color::BACKGROUND));
    g.FillRectangle(
        &bg,
        0.0f,
        static_cast<float>(Chrome::kTitleBarHeight),
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - Chrome::kTitleBarHeight)
    );

    Gdiplus::RectF track(
        static_cast<float>(kMargin),
        static_cast<float>(kTabBarTop),
        static_cast<float>(kContentWidth),
        static_cast<float>(kTabBarHeight)
    );
    Gdiplus::SolidBrush trackFill(Draw::gp(Color::SURFACE));
    Draw::fillRoundRect(g, trackFill, track, kTabTrackRadius);

    // Inset stroke so the bottom edge isn't clipped by AA / pixel grid.
    Gdiplus::RectF trackStroke = track;
    trackStroke.X += 0.5f;
    trackStroke.Y += 0.5f;
    trackStroke.Width -= 1.0f;
    trackStroke.Height -= 1.0f;
    Gdiplus::Pen trackBorder(Draw::gp(Color::BORDER), Draw::kBorder);
    Draw::drawRoundRect(g, trackBorder, trackStroke, kTabTrackRadius - 0.5f);
}

void UserInterface::drawSectionFromLabel(
    Gdiplus::Graphics& g,
    HWND label,
    Draw::Icon icon,
    const wchar_t* title
) {
    if (!label) {
        return;
    }

    RECT rc = {};
    GetWindowRect(label, &rc);
    MapWindowPoints(HWND_DESKTOP, GetParent(label), reinterpret_cast<LPPOINT>(&rc), 2);

    Gdiplus::RectF rect(
        static_cast<float>(rc.left),
        static_cast<float>(rc.top),
        static_cast<float>(rc.right - rc.left),
        static_cast<float>(rc.bottom - rc.top)
    );

    Draw::ScopedFont font(this->hFontTitle);
    Draw::drawSectionHeader(g, rect, icon, title, font.get(), static_cast<float>(kContentWidth));
}

void UserInterface::paintResolutionPage(HDC hdc) {
    RECT client = {};
    GetClientRect(this->page_resolution, &client);

    Gdiplus::Graphics g(hdc);
    Draw::configure(g);
    g.Clear(Draw::gp(Color::BACKGROUND));

    this->drawInputChrome(hdc, this->page_resolution, this->width_edit);
    this->drawInputChrome(hdc, this->page_resolution, this->height_edit);
    this->drawInputChrome(hdc, this->page_resolution, this->fps_edit);

    this->drawSectionFromLabel(g, this->display_label, Draw::Icon::Display, L"DISPLAY");
    this->drawSectionFromLabel(g, this->presets_label, Draw::Icon::Presets, L"RESOLUTIONS");
    this->drawSectionFromLabel(g, this->mode_label, Draw::Icon::Window, L"WINDOW MODE");
    this->drawTipFromControl(g, this->mode_hint, this->mode_hint_text);
}

void UserInterface::paintCrosshairPage(HDC hdc) {
    RECT client = {};
    GetClientRect(this->page_crosshair, &client);

    Gdiplus::Graphics g(hdc);
    Draw::configure(g);
    g.Clear(Draw::gp(Color::BACKGROUND));

    this->drawSectionFromLabel(g, this->cross_type_label, Draw::Icon::Crosshair, L"CROSSHAIR TYPE");

    // Shared segmented-control track behind type buttons (nested radius like main tabs)
    if (this->cross_plus && this->cross_circle) {
        RECT a = {};
        RECT b = {};
        GetWindowRect(this->cross_plus, &a);
        GetWindowRect(this->cross_circle, &b);
        MapWindowPoints(HWND_DESKTOP, this->page_crosshair, reinterpret_cast<LPPOINT>(&a), 2);
        MapWindowPoints(HWND_DESKTOP, this->page_crosshair, reinterpret_cast<LPPOINT>(&b), 2);

        Gdiplus::RectF capsule(
            static_cast<float>(a.left - kTabTrackPad),
            static_cast<float>(a.top - kTabTrackPad),
            static_cast<float>((b.right - a.left) + kTabTrackPad * 2),
            static_cast<float>((a.bottom - a.top) + kTabTrackPad * 2)
        );
        Gdiplus::SolidBrush fill(Draw::gp(Color::SURFACE));
        Draw::fillRoundRect(g, fill, capsule, kTabTrackRadius);

        Gdiplus::RectF stroke = capsule;
        stroke.X += 0.5f;
        stroke.Y += 0.5f;
        stroke.Width -= 1.0f;
        stroke.Height -= 1.0f;
        Gdiplus::Pen border(Draw::gp(Color::BORDER), Draw::kBorder);
        Draw::drawRoundRect(g, border, stroke, kTabTrackRadius - 0.5f);

        const float segW = (capsule.Width - static_cast<float>(kTabTrackPad * 2)) / 4.0f;
        Gdiplus::Pen div(Draw::gp(Color::BORDER), 1.0f);
        for (int i = 1; i < 4; ++i) {
            const float x = capsule.X + static_cast<float>(kTabTrackPad) + segW * static_cast<float>(i);
            g.DrawLine(
                &div,
                x,
                capsule.Y + static_cast<float>(kTabTrackPad) + 4.0f,
                x,
                capsule.GetBottom() - static_cast<float>(kTabTrackPad) - 4.0f
            );
        }
    }

    this->drawTipFromControl(g, this->cross_hint, this->cross_hint_text);
}

void UserInterface::drawTipFromControl(Gdiplus::Graphics& g, HWND control, const std::wstring& text) {
    if (!control || text.empty()) {
        return;
    }

    RECT rc = {};
    GetWindowRect(control, &rc);
    MapWindowPoints(HWND_DESKTOP, GetParent(control), reinterpret_cast<LPPOINT>(&rc), 2);

    Gdiplus::RectF tip(
        static_cast<float>(rc.left),
        static_cast<float>(rc.top),
        static_cast<float>(rc.right - rc.left),
        static_cast<float>(rc.bottom - rc.top)
    );

    Draw::ScopedFont font(this->hFontSmall);
    Draw::drawTipBox(g, tip, text.c_str(), font.get());
}

LRESULT UserInterface::onColorEdit(WPARAM wParam) {
    HDC hdcEdit = (HDC)wParam;
    SetTextColor(hdcEdit, Color::TEXT);
    SetBkColor(hdcEdit, Color::EDIT);
    return (LRESULT)this->hBrushEdit;
}

LRESULT UserInterface::onColorStatic(WPARAM wParam, LPARAM lParam) {
    HDC hdcStatic = (HDC)wParam;
    HWND hStatic = (HWND)lParam;
    const int id = GetDlgCtrlID(hStatic);

    SetBkMode(hdcStatic, TRANSPARENT);
    SetBkColor(hdcStatic, Color::BACKGROUND);

    switch (id) {
        case IDC_WIDTH_LABEL:
        case IDC_HEIGHT_LABEL:
        case IDC_FPS_LABEL:
        case IDC_MODE_HINT:
        case IDC_CROSS_SCALE_LABEL:
        case IDC_CROSS_COLOR_LABEL:
        case IDC_CROSS_THICK_LABEL:
        case IDC_CROSS_LENGTH_LABEL:
        case IDC_CROSS_HINT:
            SetTextColor(hdcStatic, Color::TEXT_MUTED);
            break;
        default:
            SetTextColor(hdcStatic, Color::TEXT);
            break;
    }

    return (LRESULT)this->hBrushStatic;
}

void UserInterface::drawSegmentTab(LPDRAWITEMSTRUCT pdis, int tabIndex, const wchar_t* label) {
    const bool selected = this->selected_tab == tabIndex;
    const bool pressed = (pdis->itemState & ODS_SELECTED) != 0;
    const float hov = this->hoverAmount(static_cast<int>(pdis->CtlID));

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem, Color::SURFACE);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    COLORREF fillColor = Color::SURFACE;
    COLORREF textColor = Color::TEXT_MUTED;

    if (selected) {
        fillColor = pressed ? Color::ACCENT_PRESSED : Draw::lerpColor(Color::ACCENT, Color::ACCENT_HOVER, hov);
        textColor = Color::WHITE;
    } else if (hov > 0.0f || pressed) {
        fillColor = Draw::lerpColor(Color::SURFACE, Color::SURFACE_HOVER, pressed ? 1.0f : hov);
        textColor = Draw::lerpColor(Color::TEXT_MUTED, Color::TEXT, pressed ? 1.0f : hov);
    }

    const float radius = kTabInnerRadius;
    Gdiplus::SolidBrush fill(Draw::gp(fillColor));
    Draw::fillRoundRect(g, fill, rect, radius);

    Draw::ScopedFont font(this->hFontButton);
    Gdiplus::SolidBrush text(Draw::gp(textColor));
    Draw::drawCenteredText(g, label, font.get(), text, rect);
}

void UserInterface::drawChip(LPDRAWITEMSTRUCT pdis, int width, int height, const wchar_t* label) {
    const int id = static_cast<int>(pdis->CtlID);
    const bool selected =
        this->active_preset_width == width && this->active_preset_height == height;
    const bool pressed = (pdis->itemState & ODS_SELECTED) != 0;
    const float hov = this->hoverAmount(id);

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    COLORREF fillColor = Color::SURFACE;
    COLORREF borderColor = Color::BORDER;
    COLORREF textColor = Color::TEXT;

    if (selected) {
        // Desaturated accent — soft fill, not neon orange
        fillColor = pressed ? Color::ACCENT_SOFT : Draw::lerpColor(Color::ACCENT_SOFT, Color::SURFACE_ACTIVE, hov * 0.4f);
        borderColor = Color::ACCENT_SOFT_BORDER;
        textColor = Color::ACCENT_MUTED_TEXT;
    } else if (hov > 0.0f || pressed) {
        fillColor = Draw::lerpColor(Color::SURFACE, Color::SURFACE_HOVER, pressed ? 1.0f : hov);
    }

    const float radius = rect.Height * 0.5f;
    Gdiplus::SolidBrush fill(Draw::gp(fillColor));
    Draw::fillRoundRect(g, fill, rect, radius);

    Gdiplus::Pen border(Draw::gp(borderColor), Draw::kBorder);
    Draw::drawRoundRect(g, border, rect, radius);

    Draw::ScopedFont font(this->hFontSmall);
    Gdiplus::SolidBrush text(Draw::gp(textColor));
    Draw::drawCenteredText(g, label, font.get(), text, rect);
}

void UserInterface::drawPrimaryButton(LPDRAWITEMSTRUCT pdis, int controlId, const wchar_t* label) {
    const bool pressed = (pdis->itemState & ODS_SELECTED) != 0;
    const float hov = this->hoverAmount(controlId);

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    COLORREF fillColor = Color::ACCENT;
    if (pressed) {
        fillColor = Color::ACCENT_PRESSED;
    } else if (hov > 0.0f) {
        fillColor = Draw::lerpColor(Color::ACCENT, Color::ACCENT_HOVER, hov);
    }

    Gdiplus::SolidBrush fill(Draw::gp(fillColor));
    Draw::fillRoundRect(g, fill, rect, Draw::kRadius);

    Draw::ScopedFont font(this->hFontButton);
    Gdiplus::SolidBrush text(Draw::gp(Color::WHITE));
    Draw::drawCenteredText(g, label, font.get(), text, rect);
}

void UserInterface::drawSecondaryButton(LPDRAWITEMSTRUCT pdis, int controlId, const wchar_t* label) {
    const bool pressed = (pdis->itemState & ODS_SELECTED) != 0;
    const float hov = this->hoverAmount(controlId);

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    COLORREF fillColor = Draw::lerpColor(Color::SURFACE, Color::SURFACE_HOVER, pressed ? 1.0f : hov);

    Gdiplus::SolidBrush fill(Draw::gp(fillColor));
    Draw::fillRoundRect(g, fill, rect, Draw::kRadius);

    Gdiplus::Pen border(Draw::gp(Color::BORDER), Draw::kBorder);
    Draw::drawRoundRect(g, border, rect, Draw::kRadius);

    Draw::ScopedFont font(this->hFontButton);
    Gdiplus::SolidBrush text(Draw::gp(Color::TEXT));
    Draw::drawCenteredText(g, label, font.get(), text, rect);
}

void UserInterface::drawRadio(LPDRAWITEMSTRUCT pdis, int selectedId, const wchar_t* label) {
    const int id = static_cast<int>(pdis->CtlID);
    const bool checked = (selectedId == id);
    const float hov = this->hoverAmount(id);
    const bool disabled = (pdis->itemState & ODS_DISABLED) != 0;

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    const float cx = 2.0f + kRadioOuter * 0.5f;
    const float cy = rect.Height * 0.5f;

    if (hov > 0.0f && !disabled) {
        Gdiplus::SolidBrush hoverFill(Draw::gp(Draw::lerpColor(Color::BACKGROUND, Color::SURFACE_HOVER, hov)));
        Draw::fillRoundRect(g, hoverFill, rect, Draw::kRadiusSm);
    }

    const COLORREF ring = checked
        ? Color::ACCENT
        : Draw::lerpColor(Color::BORDER, Color::TEXT_MUTED, hov);
    Gdiplus::Pen outer(Draw::gp(ring), 1.5f);
    Draw::drawCircle(g, outer, cx, cy, kRadioOuter);

    if (checked) {
        Gdiplus::SolidBrush inner(Draw::gp(Color::ACCENT));
        Draw::fillCircle(g, inner, cx, cy, kRadioInner);
    }

    Gdiplus::RectF textRect(
        cx + kRadioOuter * 0.5f + kIconTextGap,
        0.0f,
        rect.Width - (cx + kRadioOuter * 0.5f + kIconTextGap),
        rect.Height
    );

    Draw::ScopedFont font(this->hFont);
    Gdiplus::SolidBrush text(Draw::gp(disabled ? Color::TEXT_MUTED : Color::TEXT));
    Draw::drawLeftText(g, label, font.get(), text, textRect);
}

void UserInterface::drawCheckbox(LPDRAWITEMSTRUCT pdis, const wchar_t* label) {
    const bool checked = this->read_only_locked;
    const float hov = this->hoverAmount(IDC_RO_CHECKBOX);

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    const float box = static_cast<float>(kCheckSize);
    const float boxY = (rect.Height - box) * 0.5f;
    Gdiplus::RectF boxRect(0.0f, boxY, box, box);

    if (hov > 0.0f) {
        Gdiplus::SolidBrush hoverFill(Draw::gp(Draw::lerpColor(Color::BACKGROUND, Color::SURFACE_HOVER, hov)));
        Draw::fillRoundRect(g, hoverFill, rect, Draw::kRadiusSm);
    }

    Gdiplus::SolidBrush fill(Draw::gp(checked ? Color::SURFACE_ACTIVE : Color::EDIT));
    Draw::fillRoundRect(g, fill, boxRect, Draw::kCheckRadius);

    Gdiplus::Pen border(
        Draw::gp(Draw::lerpColor(Color::BORDER, Color::TEXT_MUTED, hov)),
        1.0f
    );
    Draw::drawRoundRect(g, border, boxRect, Draw::kCheckRadius);

    if (checked) {
        Gdiplus::Pen check(Draw::gp(Color::ACCENT), 2.0f);
        check.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        check.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawLine(&check, boxRect.X + 4.0f, boxRect.Y + 9.5f, boxRect.X + 7.5f, boxRect.Y + 13.0f);
        g.DrawLine(&check, boxRect.X + 7.5f, boxRect.Y + 13.0f, boxRect.X + 14.0f, boxRect.Y + 5.0f);
    }

    Gdiplus::RectF textRect(
        box + kIconTextGap,
        0.0f,
        rect.Width - box - kIconTextGap,
        rect.Height
    );

    Draw::ScopedFont font(this->hFont);
    Gdiplus::SolidBrush text(Draw::gp(Color::TEXT));
    Draw::drawLeftText(g, label, font.get(), text, textRect);
}

void UserInterface::drawColorSwatch(LPDRAWITEMSTRUCT pdis) {
    const float hov = this->hoverAmount(IDC_CROSS_COLOR);
    const bool disabled = (pdis->itemState & ODS_DISABLED) != 0;

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    Gdiplus::SolidBrush fill(Draw::gp(disabled ? Color::SURFACE : this->cross_color_value));
    Draw::fillRoundRect(g, fill, rect, Draw::kRadiusSm);

    Gdiplus::Pen border(
        Draw::gp(Draw::lerpColor(Color::BORDER, Color::TEXT_MUTED, hov)),
        Draw::kBorder
    );
    Draw::drawRoundRect(g, border, rect, Draw::kRadiusSm);
}

void UserInterface::drawTypeSegment(
    LPDRAWITEMSTRUCT pdis,
    int index,
    int count,
    CrossType type,
    const wchar_t* label
) {
    const bool selectedType = this->custom_png_path.empty() && this->cross_type == type;
    const bool pressed = (pdis->itemState & ODS_SELECTED) != 0;
    const bool disabled = (pdis->itemState & ODS_DISABLED) != 0;
    const float hov = this->hoverAmount(static_cast<int>(pdis->CtlID));

    Draw::Buffer buffer(pdis->hDC, pdis->rcItem, Color::SURFACE);
    Gdiplus::Graphics& g = buffer.graphics();
    Gdiplus::RectF rect = buffer.localRect();

    if (selectedType) {
        COLORREF fillColor = pressed
            ? Color::SURFACE_ACTIVE
            : Draw::lerpColor(Color::SURFACE_HOVER, Color::SURFACE_ACTIVE, hov);
        Gdiplus::SolidBrush fill(Draw::gp(fillColor));
        Draw::fillRoundRect(g, fill, rect, kTabInnerRadius);
    } else if ((hov > 0.0f || pressed) && !disabled) {
        Gdiplus::SolidBrush fill(Draw::gp(Draw::lerpColor(Color::SURFACE, Color::SURFACE_HOVER, pressed ? 1.0f : hov * 0.6f), 180));
        Draw::fillRoundRect(g, fill, rect, kTabInnerRadius);
    }

    (void)index;
    (void)count;

    Draw::ScopedFont font(this->hFontSmall);
    Gdiplus::SolidBrush text(Draw::gp(
        disabled ? Color::TEXT_DIM
                 : (selectedType ? Color::TEXT : Color::TEXT_MUTED)
    ));
    Draw::drawCenteredText(g, label, font.get(), text, rect);
}

LRESULT UserInterface::onDraw(WPARAM wParam, LPARAM lParam) {
    LPDRAWITEMSTRUCT pdis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);

    switch (wParam) {
        case IDC_TAB_RESOLUTION:
            this->drawSegmentTab(pdis, 0, L"Resolution");
            return TRUE;
        case IDC_TAB_CROSSHAIR:
            this->drawSegmentTab(pdis, 1, L"Crosshair");
            return TRUE;
        case IDC_APPLY_BUTTON:
            this->drawPrimaryButton(pdis, IDC_APPLY_BUTTON, L"Apply");
            return TRUE;
        case IDC_REVERT_BUTTON:
            this->drawSecondaryButton(pdis, IDC_REVERT_BUTTON, L"Revert");
            return TRUE;
        case IDC_PRESET_1440:
            this->drawChip(pdis, 1440, 1080, L"1440x1080");
            return TRUE;
        case IDC_PRESET_1560:
            this->drawChip(pdis, 1560, 1080, L"1560x1080");
            return TRUE;
        case IDC_PRESET_1550:
            this->drawChip(pdis, 1550, 1080, L"1550x1080");
            return TRUE;
        case IDC_FS_RADIO:
            this->drawRadio(pdis, this->selected_mode_id, L"Fullscreen");
            return TRUE;
        case IDC_WFS_RADIO:
            this->drawRadio(pdis, this->selected_mode_id, L"Borderless");
            return TRUE;
        case IDC_W_RADIO:
            this->drawRadio(pdis, this->selected_mode_id, L"Windowed");
            return TRUE;
        case IDC_RO_CHECKBOX:
            this->drawCheckbox(pdis, L"Lock settings file");
            return TRUE;
        case IDC_CROSS_PLUS:
            this->drawTypeSegment(pdis, 0, 4, CrossType::Plus, L"+");
            return TRUE;
        case IDC_CROSS_X:
            this->drawTypeSegment(pdis, 1, 4, CrossType::X, L"×");
            return TRUE;
        case IDC_CROSS_DOT:
            this->drawTypeSegment(pdis, 2, 4, CrossType::Dot, L"Dot");
            return TRUE;
        case IDC_CROSS_CIRCLE:
            this->drawTypeSegment(pdis, 3, 4, CrossType::Circle, L"Circle");
            return TRUE;
        case IDC_CROSS_LOAD_PNG:
            this->drawSecondaryButton(pdis, IDC_CROSS_LOAD_PNG, L"Load PNG");
            return TRUE;
        case IDC_CROSS_CLEAR_PNG:
            this->drawSecondaryButton(pdis, IDC_CROSS_CLEAR_PNG, L"Clear PNG");
            return TRUE;
        case IDC_CROSS_COLOR:
            this->drawColorSwatch(pdis);
            return TRUE;
        case IDC_CROSS_START:
            this->drawPrimaryButton(pdis, IDC_CROSS_START, L"Start");
            return TRUE;
        case IDC_CROSS_STOP:
            this->drawSecondaryButton(pdis, IDC_CROSS_STOP, L"Stop");
            return TRUE;
        default:
            return FALSE;
    }
}
