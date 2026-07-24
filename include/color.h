#pragma once

#include <windows.h>

namespace Color {
    // Surfaces
    const COLORREF BACKGROUND = RGB(0x14, 0x14, 0x14);
    const COLORREF SURFACE = RGB(0x1C, 0x1C, 0x1C);
    const COLORREF SURFACE_HOVER = RGB(0x26, 0x26, 0x26);
    const COLORREF SURFACE_ACTIVE = RGB(0x2E, 0x2E, 0x2E);
    const COLORREF EDIT = RGB(0x1F, 0x1F, 0x1F);
    const COLORREF BORDER = RGB(0x32, 0x32, 0x32);
    const COLORREF BORDER_SOFT = RGB(0x2A, 0x2A, 0x2A);

    // Text
    const COLORREF TEXT = RGB(0xF2, 0xF2, 0xF2);
    const COLORREF TEXT_MUTED = RGB(0xA0, 0xA0, 0xA0);
    const COLORREF TEXT_DIM = RGB(0x78, 0x78, 0x78);
    const COLORREF WHITE = RGB(255, 255, 255);

    // Accent — reserved for tabs, primary actions, radio selection
    const COLORREF ACCENT = RGB(0xF5, 0x7C, 0x00);
    const COLORREF ACCENT_HOVER = RGB(0xFF, 0x8A, 0x14);
    const COLORREF ACCENT_PRESSED = RGB(0xE0, 0x6E, 0x00);
    const COLORREF ACCENT_SOFT = RGB(0x3A, 0x28, 0x18);
    const COLORREF ACCENT_SOFT_BORDER = RGB(0x6B, 0x42, 0x18);
    const COLORREF ACCENT_MUTED_TEXT = RGB(0xE8, 0xB0, 0x6A);

    // Icons / tracks
    const COLORREF ICON = RGB(0x7A, 0x7A, 0x7A);
    const COLORREF TRACK = RGB(0x44, 0x44, 0x44);

    // Hint / alert
    const COLORREF HINT_FILL = RGB(0x1A, 0x1A, 0x1A);
    const COLORREF HINT_TEXT = RGB(0xD0, 0xC8, 0xBE);
}
