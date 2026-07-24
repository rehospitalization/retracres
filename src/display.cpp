#include "display.h"

#include <fstream>
#include <sstream>

namespace {
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
}

std::wstring DisplayMode::backupPath() {
    wchar_t appData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"";
    }
    return std::wstring(appData) + L"\\RetracRes\\display_backup.json";
}

bool DisplayMode::captureCurrent() {
    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode)) {
        return false;
    }

    this->backup = mode;
    this->backupValid = true;
    return this->saveBackup();
}

bool DisplayMode::hasBackup() const {
    return this->backupValid;
}

int DisplayMode::backupWidth() const {
    return static_cast<int>(this->backup.dmPelsWidth);
}

int DisplayMode::backupHeight() const {
    return static_cast<int>(this->backup.dmPelsHeight);
}

bool DisplayMode::modeAvailable(int width, int height) const {
    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);

    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &mode); ++i) {
        if (static_cast<int>(mode.dmPelsWidth) == width
            && static_cast<int>(mode.dmPelsHeight) == height) {
            return true;
        }
    }

    return false;
}

bool DisplayMode::apply(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    DEVMODEW current = {};
    current.dmSize = sizeof(current);
    if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &current)) {
        return false;
    }

    if (static_cast<int>(current.dmPelsWidth) == width
        && static_cast<int>(current.dmPelsHeight) == height) {
        return true;
    }

    if (!this->backupValid) {
        this->captureCurrent();
    }

    if (!this->modeAvailable(width, height)) {
        return false;
    }

    DEVMODEW target = current;
    target.dmPelsWidth = static_cast<DWORD>(width);
    target.dmPelsHeight = static_cast<DWORD>(height);
    target.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    if (ChangeDisplaySettingsW(&target, CDS_TEST) != DISP_CHANGE_SUCCESSFUL) {
        return false;
    }

    return ChangeDisplaySettingsW(&target, CDS_UPDATEREGISTRY) == DISP_CHANGE_SUCCESSFUL;
}

bool DisplayMode::restore() {
    if (!this->backupValid && !this->loadBackup()) {
        return false;
    }

    DEVMODEW current = {};
    current.dmSize = sizeof(current);
    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &current);

    if (current.dmPelsWidth == this->backup.dmPelsWidth
        && current.dmPelsHeight == this->backup.dmPelsHeight) {
        return true;
    }

    DEVMODEW target = this->backup;
    target.dmSize = sizeof(target);
    target.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    if (ChangeDisplaySettingsW(&target, CDS_TEST) != DISP_CHANGE_SUCCESSFUL) {
        target.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    }

    return ChangeDisplaySettingsW(&target, CDS_UPDATEREGISTRY) == DISP_CHANGE_SUCCESSFUL;
}

bool DisplayMode::saveBackup() const {
    if (!this->backupValid) {
        return false;
    }

    const std::wstring path = backupPath();
    if (path.empty()) {
        return false;
    }

    const std::wstring dir = path.substr(0, path.find_last_of(L'\\'));
    CreateDirectoryW(dir.c_str(), nullptr);

    std::ostringstream json;
    json << "{"
         << "\"width\":" << this->backup.dmPelsWidth << ","
         << "\"height\":" << this->backup.dmPelsHeight << ","
         << "\"bits\":" << this->backup.dmBitsPerPel << ","
         << "\"freq\":" << this->backup.dmDisplayFrequency
         << "}";

    std::ofstream file(narrow(path), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    const std::string utf8 = json.str();
    file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(file);
}

bool DisplayMode::loadBackup() {
    const std::wstring path = backupPath();
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

    auto readInt = [&](const wchar_t* key, int fallback) {
        const std::wstring needle = std::wstring(L"\"") + key + L"\"";
        size_t pos = json.find(needle);
        if (pos == std::wstring::npos) {
            return fallback;
        }
        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) {
            return fallback;
        }
        try {
            return std::stoi(json.substr(pos + 1));
        } catch (...) {
            return fallback;
        }
    };

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode);

    mode.dmPelsWidth = static_cast<DWORD>(readInt(L"width", static_cast<int>(mode.dmPelsWidth)));
    mode.dmPelsHeight = static_cast<DWORD>(readInt(L"height", static_cast<int>(mode.dmPelsHeight)));
    mode.dmBitsPerPel = static_cast<DWORD>(readInt(L"bits", static_cast<int>(mode.dmBitsPerPel)));
    mode.dmDisplayFrequency = static_cast<DWORD>(readInt(L"freq", static_cast<int>(mode.dmDisplayFrequency)));
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    this->backup = mode;
    this->backupValid = mode.dmPelsWidth > 0 && mode.dmPelsHeight > 0;
    return this->backupValid;
}
