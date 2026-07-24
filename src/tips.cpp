#include "tips.h"

#include <commctrl.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
std::wstring tipsPath() {
    wchar_t appData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"";
    }
    return std::wstring(appData) + L"\\RetracRes\\tips.json";
}

std::string narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 1) {
        return {};
    }
    std::wstring out(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
    return out;
}

bool readHiddenFlag() {
    const std::wstring path = tipsPath();
    if (path.empty()) {
        return false;
    }

    std::ifstream file(narrow(path), std::ios::binary);
    if (!file) {
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    const std::wstring json = widen(oss.str());
    return json.find(L"\"hide_borderless_tip\":true") != std::wstring::npos
        || json.find(L"\"hide_borderless_tip\": true") != std::wstring::npos;
}

void writeHiddenFlag(bool hidden) {
    const std::wstring path = tipsPath();
    if (path.empty()) {
        return;
    }

    const std::wstring dir = path.substr(0, path.find_last_of(L'\\'));
    CreateDirectoryW(dir.c_str(), nullptr);

    std::ofstream file(narrow(path), std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }

    const char* json = hidden
        ? "{\"hide_borderless_tip\":true}"
        : "{\"hide_borderless_tip\":false}";
    file.write(json, static_cast<std::streamsize>(strlen(json)));
}
}

namespace Tips {
bool shouldShowBorderlessTip() {
    return !readHiddenFlag();
}

void setBorderlessTipHidden(bool hidden) {
    writeHiddenFlag(hidden);
}

void showBorderlessTip(HWND owner) {
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = L"Borderless / Windowed";
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = L"How Borderless and Windowed work";
    config.pszContent =
        L"Fortnite uses the Windows desktop size in Borderless and Windowed modes.\n\n"
        L"RetracRes will switch Windows to your Width x Height when you press Apply.\n\n"
        L"Create that resolution first in NVIDIA/AMD control panel, and set GPU scaling "
        L"to Full screen / Full panel.\n\n"
        L"The crosshair overlay also needs Borderless or Windowed — it will not show "
        L"over Exclusive Fullscreen.";
    config.pszVerificationText = L"Don't show again";

    BOOL verification = FALSE;
    TaskDialogIndirect(&config, nullptr, nullptr, &verification);
    if (verification) {
        setBorderlessTipHidden(true);
    }
}
}
