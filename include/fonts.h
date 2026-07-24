#pragma once

#include <string>
#include <vector>
#include <windows.h>

class AppFonts {
public:
    AppFonts();
    ~AppFonts();

    AppFonts(const AppFonts&) = delete;
    AppFonts& operator=(const AppFonts&) = delete;

    bool load();
    void unload();
    bool isLoaded() const;
    const wchar_t* faceName() const;

    HFONT create(int heightPx, int weight) const;

private:
    std::vector<std::wstring> loadedPaths;
    bool loaded = false;

    static std::wstring fontsDirectory();
    bool addFile(const std::wstring& path);
};
