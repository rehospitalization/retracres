#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>

#include "color.h"

namespace Draw {
    inline constexpr float kRadius = 10.0f;
    inline constexpr float kRadiusSm = 8.0f;
    inline constexpr float kBorder = 1.0f;
    inline constexpr float kHintRadius = 10.0f;
    inline constexpr float kIconSize = 18.0f;
    inline constexpr float kIconStroke = 1.85f;
    inline constexpr float kCheckRadius = 5.0f;

    enum class Icon {
        Display,
        Presets,
        Window,
        Crosshair,
        Info
    };

    inline Gdiplus::Color gp(COLORREF c, BYTE alpha = 255) {
        return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
    }

    inline COLORREF lerpColor(COLORREF a, COLORREF b, float t) {
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        const auto mix = [t](BYTE x, BYTE y) -> BYTE {
            return static_cast<BYTE>(std::lround(x + (y - x) * t));
        };
        return RGB(
            mix(GetRValue(a), GetRValue(b)),
            mix(GetGValue(a), GetGValue(b)),
            mix(GetBValue(a), GetBValue(b))
        );
    }

    inline void configure(Gdiplus::Graphics& g) {
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        // AntiAlias (no grid-fit) is smoother on dark UI / DIB back-buffers.
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    }

    inline void addRoundRect(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius) {
        const float r = (radius * 2.0f > rect.Width) ? rect.Width / 2.0f : radius;
        const float r2 = (r * 2.0f > rect.Height) ? rect.Height / 2.0f : r;
        const float dia = r2 * 2.0f;

        path.AddArc(rect.X, rect.Y, dia, dia, 180.0f, 90.0f);
        path.AddArc(rect.GetRight() - dia, rect.Y, dia, dia, 270.0f, 90.0f);
        path.AddArc(rect.GetRight() - dia, rect.GetBottom() - dia, dia, dia, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.GetBottom() - dia, dia, dia, 90.0f, 90.0f);
        path.CloseFigure();
    }

    // Corners: bit0=TL, bit1=TR, bit2=BR, bit3=BL
    inline void addRoundRectCorners(
        Gdiplus::GraphicsPath& path,
        Gdiplus::RectF rect,
        float radius,
        int corners
    ) {
        const float r = (std::min)(radius, (std::min)(rect.Width, rect.Height) * 0.5f);
        const float d = r * 2.0f;
        const float x = rect.X;
        const float y = rect.Y;
        const float w = rect.Width;
        const float h = rect.Height;

        if (corners & 1) {
            path.AddArc(x, y, d, d, 180.0f, 90.0f);
        } else {
            path.AddLine(x, y, x + r, y);
        }

        if (corners & 2) {
            path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        } else {
            path.AddLine(x + w - r, y, x + w, y);
        }

        if (corners & 4) {
            path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        } else {
            path.AddLine(x + w, y + h - r, x + w, y + h);
        }

        if (corners & 8) {
            path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        } else {
            path.AddLine(x + r, y + h, x, y + h);
        }

        path.CloseFigure();
    }

    inline void fillRoundRect(
        Gdiplus::Graphics& g,
        const Gdiplus::Brush& brush,
        Gdiplus::RectF rect,
        float radius
    ) {
        Gdiplus::GraphicsPath path;
        addRoundRect(path, rect, radius);
        g.FillPath(&brush, &path);
    }

    inline void fillRoundRectCorners(
        Gdiplus::Graphics& g,
        const Gdiplus::Brush& brush,
        Gdiplus::RectF rect,
        float radius,
        int corners
    ) {
        Gdiplus::GraphicsPath path;
        addRoundRectCorners(path, rect, radius, corners);
        g.FillPath(&brush, &path);
    }

    inline void drawRoundRect(
        Gdiplus::Graphics& g,
        const Gdiplus::Pen& pen,
        Gdiplus::RectF rect,
        float radius
    ) {
        const float inset = pen.GetWidth() * 0.5f;
        rect.X += inset;
        rect.Y += inset;
        rect.Width -= pen.GetWidth();
        rect.Height -= pen.GetWidth();

        Gdiplus::GraphicsPath path;
        addRoundRect(path, rect, radius);
        g.DrawPath(&pen, &path);
    }

    inline void fillCircle(Gdiplus::Graphics& g, const Gdiplus::Brush& brush, float cx, float cy, float diameter) {
        g.FillEllipse(&brush, cx - diameter * 0.5f, cy - diameter * 0.5f, diameter, diameter);
    }

    inline void drawCircle(Gdiplus::Graphics& g, const Gdiplus::Pen& pen, float cx, float cy, float diameter) {
        const float inset = pen.GetWidth() * 0.5f;
        g.DrawEllipse(
            &pen,
            cx - diameter * 0.5f + inset,
            cy - diameter * 0.5f + inset,
            diameter - pen.GetWidth(),
            diameter - pen.GetWidth()
        );
    }

    inline void drawCenteredText(
        Gdiplus::Graphics& g,
        const wchar_t* text,
        const Gdiplus::Font& font,
        const Gdiplus::Brush& brush,
        const Gdiplus::RectF& rect
    ) {
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        g.DrawString(text, -1, &font, rect, &format, &brush);
    }

    inline void drawLeftText(
        Gdiplus::Graphics& g,
        const wchar_t* text,
        const Gdiplus::Font& font,
        const Gdiplus::Brush& brush,
        const Gdiplus::RectF& rect
    ) {
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        g.DrawString(text, -1, &font, rect, &format, &brush);
    }

    inline void drawWrappedText(
        Gdiplus::Graphics& g,
        const wchar_t* text,
        const Gdiplus::Font& font,
        const Gdiplus::Brush& brush,
        const Gdiplus::RectF& rect
    ) {
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentNear);
        g.DrawString(text, -1, &font, rect, &format, &brush);
    }

    inline void drawIcon(Gdiplus::Graphics& g, Icon icon, float x, float y, COLORREF color = Color::ICON) {
        const float s = kIconSize;
        Gdiplus::Pen pen(gp(color), kIconStroke);
        pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::SolidBrush brush(gp(color));

        switch (icon) {
            case Icon::Display: {
                // monitor
                Gdiplus::RectF screen(x + 1.5f, y + 2.0f, s - 3.0f, s - 7.5f);
                drawRoundRect(g, pen, screen, 2.0f);
                g.DrawLine(&pen, x + s * 0.5f, y + s - 5.0f, x + s * 0.5f, y + s - 2.5f);
                g.DrawLine(&pen, x + 4.0f, y + s - 2.5f, x + s - 4.0f, y + s - 2.5f);
                break;
            }
            case Icon::Presets: {
                // layout-grid
                const float gap = 2.2f;
                const float cell = (s - 3.0f - gap) * 0.5f;
                for (int row = 0; row < 2; ++row) {
                    for (int col = 0; col < 2; ++col) {
                        Gdiplus::RectF cellRect(
                            x + 1.5f + col * (cell + gap),
                            y + 1.5f + row * (cell + gap),
                            cell,
                            cell
                        );
                        drawRoundRect(g, pen, cellRect, 1.5f);
                    }
                }
                break;
            }
            case Icon::Window: {
                // app-window
                Gdiplus::RectF frame(x + 1.5f, y + 2.0f, s - 3.0f, s - 4.0f);
                drawRoundRect(g, pen, frame, 2.0f);
                g.DrawLine(&pen, frame.X + 1.0f, frame.Y + 4.0f, frame.GetRight() - 1.0f, frame.Y + 4.0f);
                break;
            }
            case Icon::Crosshair: {
                const float cx = x + s * 0.5f;
                const float cy = y + s * 0.5f;
                g.DrawLine(&pen, cx - 5.5f, cy, cx + 5.5f, cy);
                g.DrawLine(&pen, cx, cy - 5.5f, cx, cy + 5.5f);
                drawCircle(g, pen, cx, cy, 5.0f);
                break;
            }
            case Icon::Info: {
                // circle-alert (smaller)
                const float cx = x + s * 0.5f;
                const float cy = y + s * 0.5f;
                drawCircle(g, pen, cx, cy, s - 3.0f);
                g.DrawLine(&pen, cx, cy - 2.2f, cx, cy + 1.8f);
                fillCircle(g, brush, cx, cy + 3.6f, 1.6f);
                break;
            }
        }
    }

    inline void drawSectionHeader(
        Gdiplus::Graphics& g,
        const Gdiplus::RectF& labelRect,
        Icon icon,
        const wchar_t* title,
        const Gdiplus::Font& font,
        float contentWidth
    ) {
        const float iconX = labelRect.X - 22.0f;
        const float iconY = labelRect.Y + (labelRect.Height - kIconSize) * 0.5f;
        drawIcon(g, icon, iconX, (std::max)(1.0f, iconY), Color::ICON);

        Gdiplus::SolidBrush text(gp(Color::TEXT_MUTED));
        drawLeftText(g, title, font, text, labelRect);

        const float lineY = labelRect.GetBottom() + 6.0f;
        Gdiplus::Pen hairline(gp(Color::BORDER_SOFT), 1.0f);
        g.DrawLine(&hairline, labelRect.X - 22.0f, lineY, labelRect.X - 22.0f + contentWidth, lineY);
    }

    inline constexpr float kTipPadX = 34.0f;
    inline constexpr float kTipPadRight = 12.0f;
    inline constexpr float kTipPadY = 12.0f;

    inline int measureTipHeight(
        Gdiplus::Graphics& g,
        const wchar_t* text,
        const Gdiplus::Font& font,
        float tipWidth
    ) {
        if (!text || !text[0]) {
            return static_cast<int>(kIconSize + kTipPadY * 2.0f);
        }

        const float textWidth = (std::max)(40.0f, tipWidth - kTipPadX - kTipPadRight);
        Gdiplus::RectF layout(0.0f, 0.0f, textWidth, 2000.0f);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentNear);

        Gdiplus::RectF bounds;
        g.MeasureString(text, -1, &font, layout, &format, &bounds);

        const float content = (std::max)(bounds.Height, kIconSize);
        const float height = content + kTipPadY * 2.0f;
        return static_cast<int>(std::ceil(height));
    }

    inline void drawTipBox(
        Gdiplus::Graphics& g,
        const Gdiplus::RectF& rect,
        const wchar_t* text,
        const Gdiplus::Font& font
    ) {
        Gdiplus::SolidBrush fill(gp(Color::HINT_FILL));
        fillRoundRect(g, fill, rect, kHintRadius);

        Gdiplus::SolidBrush bar(gp(Color::ACCENT));
        Gdiplus::RectF accent(rect.X, rect.Y + 6.0f, 2.0f, rect.Height - 12.0f);
        fillRoundRect(g, bar, accent, 1.0f);

        drawIcon(g, Icon::Info, rect.X + 12.0f, rect.Y + (rect.Height - kIconSize) * 0.5f, Color::ICON);

        Gdiplus::RectF textRect(
            rect.X + kTipPadX,
            rect.Y + kTipPadY,
            rect.Width - kTipPadX - kTipPadRight,
            rect.Height - kTipPadY * 2.0f
        );
        Gdiplus::SolidBrush textBrush(gp(Color::HINT_TEXT));
        drawWrappedText(g, text, font, textBrush, textRect);
    }

    inline void drawSlider(
        Gdiplus::Graphics& g,
        const Gdiplus::RectF& bounds,
        float value01,
        bool enabled
    ) {
        value01 = (std::max)(0.0f, (std::min)(1.0f, value01));

        const float cy = bounds.Y + bounds.Height * 0.5f;
        const float trackH = 4.0f;
        const float thumb = 16.0f;
        const float pad = thumb * 0.5f;
        const float trackL = bounds.X + pad;
        const float trackR = bounds.GetRight() - pad;
        const float trackW = trackR - trackL;

        Gdiplus::RectF track(trackL, cy - trackH * 0.5f, trackW, trackH);
        Gdiplus::SolidBrush trackBrush(gp(enabled ? Color::TRACK : Color::BORDER));
        fillRoundRect(g, trackBrush, track, 1.5f);

        if (enabled && value01 > 0.0f) {
            Gdiplus::RectF active(trackL, track.Y, trackW * value01, trackH);
            Gdiplus::SolidBrush activeBrush(gp(Color::ACCENT));
            fillRoundRect(g, activeBrush, active, 1.5f);
        }

        const float tx = trackL + trackW * value01;
        // Soft shadow
        Gdiplus::SolidBrush shadow(gp(RGB(0, 0, 0), enabled ? 60 : 30));
        fillCircle(g, shadow, tx, cy + 1.0f, thumb + 1.0f);

        Gdiplus::SolidBrush thumbBrush(gp(enabled ? Color::ACCENT : Color::TEXT_DIM));
        fillCircle(g, thumbBrush, tx, cy, thumb);
    }

    class ScopedFont {
    public:
        explicit ScopedFont(HFONT hfont) {
            LOGFONTW lf = {};
            GetObjectW(hfont, sizeof(lf), &lf);
            HDC screen = GetDC(nullptr);
            font_ = new Gdiplus::Font(screen, &lf);
            ReleaseDC(nullptr, screen);
        }

        ~ScopedFont() {
            delete font_;
        }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;

        Gdiplus::Font& get() { return *font_; }

    private:
        Gdiplus::Font* font_ = nullptr;
    };

    class Buffer {
    public:
        Buffer(HDC target, const RECT& rc, COLORREF clear = Color::BACKGROUND)
            : target_(target),
              width_(rc.right - rc.left),
              height_(rc.bottom - rc.top),
              left_(rc.left),
              top_(rc.top) {
            memDc_ = CreateCompatibleDC(target_);
            bmi_ = {};
            bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi_.bmiHeader.biWidth = width_;
            bmi_.bmiHeader.biHeight = -height_;
            bmi_.bmiHeader.biPlanes = 1;
            bmi_.bmiHeader.biBitCount = 32;
            bmi_.bmiHeader.biCompression = BI_RGB;

            bits_ = nullptr;
            bitmap_ = CreateDIBSection(memDc_, &bmi_, DIB_RGB_COLORS, &bits_, nullptr, 0);
            old_ = SelectObject(memDc_, bitmap_);
            graphics_ = new Gdiplus::Graphics(memDc_);
            configure(*graphics_);
            graphics_->Clear(gp(clear));
        }

        ~Buffer() {
            graphics_->Flush(Gdiplus::FlushIntentionSync);
            BitBlt(target_, left_, top_, width_, height_, memDc_, 0, 0, SRCCOPY);
            delete graphics_;
            SelectObject(memDc_, old_);
            DeleteObject(bitmap_);
            DeleteDC(memDc_);
        }

        Gdiplus::Graphics& graphics() { return *graphics_; }
        Gdiplus::RectF localRect() const {
            return Gdiplus::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));
        }

    private:
        HDC target_;
        HDC memDc_;
        HBITMAP bitmap_;
        HGDIOBJ old_;
        BITMAPINFO bmi_;
        void* bits_;
        Gdiplus::Graphics* graphics_;
        int width_;
        int height_;
        int left_;
        int top_;
    };
}
