#include "fonts.h"

#include <filesystem>

namespace {
constexpr DWORD kFrPrivate = 0x10;
}

AppFonts::AppFonts() = default;

AppFonts::~AppFonts() {
    this->unload();
}

bool AppFonts::isLoaded() const {
    return this->loaded;
}

const wchar_t* AppFonts::faceName() const {
    return this->loaded ? L"SF Pro Display" : L"Segoe UI";
}

std::wstring AppFonts::fontsDirectory() {
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        return L"";
    }

    std::filesystem::path exe(modulePath);
    const std::filesystem::path candidates[] = {
        exe.parent_path() / L"font",
        exe.parent_path().parent_path() / L"font",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::is_directory(candidate)) {
            return candidate.wstring();
        }
    }

    return L"";
}

bool AppFonts::addFile(const std::wstring& path) {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    if (AddFontResourceExW(path.c_str(), kFrPrivate, 0) <= 0) {
        return false;
    }

    this->loadedPaths.push_back(path);
    return true;
}

bool AppFonts::load() {
    this->unload();

    const std::wstring dir = fontsDirectory();
    if (dir.empty()) {
        return false;
    }

    // Regular for labels/body, Medium for buttons, Bold for section titles.
    const wchar_t* files[] = {
        L"SFPRODISPLAYREGULAR.OTF",
        L"SFPRODISPLAYMEDIUM.OTF",
        L"SFPRODISPLAYBOLD.OTF",
    };

    for (const wchar_t* file : files) {
        this->addFile(dir + L"\\" + file);
    }

    this->loaded = !this->loadedPaths.empty();
    return this->loaded;
}

void AppFonts::unload() {
    for (const std::wstring& path : this->loadedPaths) {
        RemoveFontResourceExW(path.c_str(), kFrPrivate, 0);
    }
    this->loadedPaths.clear();
    this->loaded = false;
}

HFONT AppFonts::create(int heightPx, int weight) const {
    return CreateFontW(
        -heightPx,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        this->faceName()
    );
}
