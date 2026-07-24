#include "overlay.h"
#include "draw.h"
#include "color.h"

#include <string>

namespace {
constexpr COLORREF kTransparentKey = RGB(0, 0, 0);
constexpr UINT_PTR kOverlayTimer = 1;

void drawCrosshairShapes(Gdiplus::Graphics& g, const CrosshairSettings* settings, int width, int height) {
    if (!settings) {
        return;
    }

    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    const float length = static_cast<float>(settings->length);
    const float thickness = static_cast<float>(settings->thickness);
    const Gdiplus::Color color = Draw::gp(settings->color);

    Gdiplus::Pen pen(color, thickness);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);

    switch (settings->type) {
        case CrossType::Plus:
            g.DrawLine(&pen, cx - length, cy, cx + length, cy);
            g.DrawLine(&pen, cx, cy - length, cx, cy + length);
            break;
        case CrossType::X:
            g.DrawLine(&pen, cx - length, cy - length, cx + length, cy + length);
            g.DrawLine(&pen, cx - length, cy + length, cx + length, cy - length);
            break;
        case CrossType::Dot: {
            Gdiplus::SolidBrush brush(color);
            Draw::fillCircle(g, brush, cx, cy, thickness);
            break;
        }
        case CrossType::Circle:
            Draw::drawCircle(g, pen, cx, cy, length * 2.0f);
            break;
    }
}
}

Overlay::Overlay() = default;

Overlay::~Overlay() {
    this->destroy();
}

bool Overlay::isActive() const {
    return this->hWnd != nullptr;
}

Overlay* Overlay::getObject(HWND hwnd) {
    return reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void Overlay::registerClass() {
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = Overlay::WindowProc;
    wcex.hInstance = this->hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = L"RetracResOverlay";

    if (!RegisterClassExW(&wcex)) {
        // Already registered in this process is fine.
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    registered = true;
}

void Overlay::clearImage() {
    delete this->customImage;
    this->customImage = nullptr;
}

void Overlay::reloadImage() {
    this->clearImage();
    if (!this->settings || this->settings->customPngPath.empty()) {
        return;
    }

    this->customImage = new Gdiplus::Image(this->settings->customPngPath.c_str());
    if (this->customImage->GetLastStatus() != Gdiplus::Ok) {
        this->clearImage();
    }
}

bool Overlay::create(HINSTANCE instance, const CrosshairSettings* settings, int width, int height) {
    if (this->hWnd || !settings || width <= 0 || height <= 0) {
        return false;
    }

    this->hInstance = instance;
    this->settings = settings;
    this->width = width;
    this->height = height;
    this->registerClass();
    this->reloadImage();

    this->hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RetracResOverlay",
        L"RetracRes Overlay",
        WS_POPUP,
        0,
        0,
        width,
        height,
        nullptr,
        nullptr,
        this->hInstance,
        this
    );

    if (!this->hWnd) {
        this->clearImage();
        return false;
    }

    SetLayeredWindowAttributes(this->hWnd, kTransparentKey, 0, LWA_COLORKEY);
    ShowWindow(this->hWnd, SW_SHOWNOACTIVATE);
    SetTimer(this->hWnd, kOverlayTimer, 33, nullptr);
    this->refresh();
    return true;
}

void Overlay::destroy() {
    if (this->hWnd) {
        KillTimer(this->hWnd, kOverlayTimer);
        DestroyWindow(this->hWnd);
        this->hWnd = nullptr;
    }
    this->clearImage();
    this->settings = nullptr;
}

void Overlay::setSize(int width, int height) {
    if (!this->hWnd || width <= 0 || height <= 0) {
        return;
    }

    this->width = width;
    this->height = height;
    SetWindowPos(this->hWnd, HWND_TOPMOST, 0, 0, width, height, SWP_NOACTIVATE);
    this->refresh();
}

void Overlay::refresh() {
    if (this->hWnd) {
        InvalidateRect(this->hWnd, nullptr, TRUE);
    }
}

void Overlay::paint(HDC hdc) {
    Gdiplus::Graphics g(hdc);
    Draw::configure(g);
    g.Clear(Draw::gp(kTransparentKey));

    if (!this->settings) {
        return;
    }

    if (this->customImage) {
        const float scale = this->settings->customScale / 100.0f;
        const float imgW = static_cast<float>(this->customImage->GetWidth()) * scale;
        const float imgH = static_cast<float>(this->customImage->GetHeight()) * scale;
        const float x = this->width * 0.5f - imgW * 0.5f;
        const float y = this->height * 0.5f - imgH * 0.5f;
        g.DrawImage(this->customImage, x, y, imgW, imgH);
        return;
    }

    drawCrosshairShapes(g, this->settings, this->width, this->height);
}

LRESULT CALLBACK Overlay::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Overlay* overlay = nullptr;

    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        overlay = reinterpret_cast<Overlay*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        overlay->hWnd = hwnd;
    } else {
        overlay = getObject(hwnd);
    }

    if (!overlay) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER:
            if (wParam == kOverlayTimer) {
                overlay->refresh();
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            overlay->paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, kOverlayTimer);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
