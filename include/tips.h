#pragma once

#include <windows.h>

namespace Tips {
    bool shouldShowBorderlessTip();
    void setBorderlessTipHidden(bool hidden);
    void showBorderlessTip(HWND owner);
}
