#pragma once

#include <string>
#include <windows.h>

enum class CrossType {
    Plus = 0,
    X = 1,
    Dot = 2,
    Circle = 3
};

class CrosshairSettings {
public:
    CrosshairSettings();

    CrossType type = CrossType::Plus;
    COLORREF color = RGB(0xF5, 0x7C, 0x00);
    int thickness = 2;
    int length = 20;
    int customScale = 100;
    std::wstring customPngPath;

    bool hasCustomImage() const;
    void clearCustomImage();

    bool load();
    bool save() const;

    static const wchar_t* typeToString(CrossType type);
    static CrossType typeFromString(const std::wstring& value);
    static std::wstring configDirectory();
    static std::wstring configPath();

private:
    static std::wstring colorToHex(COLORREF color);
    static bool colorFromHex(const std::wstring& hex, COLORREF* out);
};
