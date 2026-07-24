#include "crosshair.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::wstring trim(const std::wstring& value) {
    size_t start = 0;
    while (start < value.size() && iswspace(value[start])) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && iswspace(value[end - 1])) {
        --end;
    }

    return value.substr(start, end - start);
}

std::wstring extractJsonString(const std::wstring& json, const std::wstring& key) {
    const std::wstring needle = L"\"" + key + L"\"";
    size_t pos = json.find(needle);
    if (pos == std::wstring::npos) {
        return L"";
    }

    pos = json.find(L':', pos + needle.size());
    if (pos == std::wstring::npos) {
        return L"";
    }

    pos = json.find_first_not_of(L" \t\r\n", pos + 1);
    if (pos == std::wstring::npos) {
        return L"";
    }

    if (json[pos] == L'"') {
        size_t end = pos + 1;
        std::wstring out;
        while (end < json.size() && json[end] != L'"') {
            if (json[end] == L'\\' && end + 1 < json.size()) {
                out.push_back(json[end + 1]);
                end += 2;
                continue;
            }
            out.push_back(json[end]);
            ++end;
        }
        return out;
    }

    size_t end = pos;
    while (end < json.size() && json[end] != L',' && json[end] != L'}' && !iswspace(json[end])) {
        ++end;
    }
    return trim(json.substr(pos, end - pos));
}

int extractJsonInt(const std::wstring& json, const std::wstring& key, int fallback) {
    const std::wstring value = extractJsonString(json, key);
    if (value.empty()) {
        return fallback;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::wstring escapeJson(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t ch : value) {
        if (ch == L'\\' || ch == L'"') {
            out.push_back(L'\\');
        }
        out.push_back(ch);
    }
    return out;
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
}

CrosshairSettings::CrosshairSettings() = default;

bool CrosshairSettings::hasCustomImage() const {
    return !this->customPngPath.empty();
}

void CrosshairSettings::clearCustomImage() {
    this->customPngPath.clear();
}

const wchar_t* CrosshairSettings::typeToString(CrossType type) {
    switch (type) {
        case CrossType::X:
            return L"x";
        case CrossType::Dot:
            return L"dot";
        case CrossType::Circle:
            return L"circle";
        case CrossType::Plus:
        default:
            return L"+";
    }
}

CrossType CrosshairSettings::typeFromString(const std::wstring& value) {
    if (value == L"x") {
        return CrossType::X;
    }
    if (value == L"dot") {
        return CrossType::Dot;
    }
    if (value == L"circle") {
        return CrossType::Circle;
    }
    return CrossType::Plus;
}

std::wstring CrosshairSettings::configDirectory() {
    wchar_t appData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"";
    }

    return std::wstring(appData) + L"\\RetracRes";
}

std::wstring CrosshairSettings::configPath() {
    const std::wstring dir = configDirectory();
    if (dir.empty()) {
        return L"";
    }
    return dir + L"\\crosshair.json";
}

std::wstring CrosshairSettings::colorToHex(COLORREF color) {
    wchar_t buffer[16] = {};
    swprintf_s(
        buffer,
        L"#%02X%02X%02X",
        GetRValue(color),
        GetGValue(color),
        GetBValue(color)
    );
    return buffer;
}

bool CrosshairSettings::colorFromHex(const std::wstring& hex, COLORREF* out) {
    if (!out || hex.size() < 7 || hex[0] != L'#') {
        return false;
    }

    auto nibble = [](wchar_t ch) -> int {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        return -1;
    };

    int values[6] = {};
    for (int i = 0; i < 6; ++i) {
        values[i] = nibble(hex[static_cast<size_t>(i + 1)]);
        if (values[i] < 0) {
            return false;
        }
    }

    *out = RGB(
        values[0] * 16 + values[1],
        values[2] * 16 + values[3],
        values[4] * 16 + values[5]
    );
    return true;
}

bool CrosshairSettings::load() {
    const std::wstring path = configPath();
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

    this->type = typeFromString(extractJsonString(json, L"cross_type"));
    this->thickness = extractJsonInt(json, L"thickness", this->thickness);
    this->length = extractJsonInt(json, L"length", this->length);
    this->customScale = extractJsonInt(json, L"custom_scale", this->customScale);

    COLORREF parsed = this->color;
    if (colorFromHex(extractJsonString(json, L"color"), &parsed)) {
        this->color = parsed;
    }

    this->customPngPath = extractJsonString(json, L"custom_png_path");

    if (this->thickness < 1) this->thickness = 1;
    if (this->thickness > 10) this->thickness = 10;
    if (this->length < 5) this->length = 5;
    if (this->length > 100) this->length = 100;
    if (this->customScale < 10) this->customScale = 10;
    if (this->customScale > 300) this->customScale = 300;

    return true;
}

bool CrosshairSettings::save() const {
    const std::wstring dir = configDirectory();
    const std::wstring path = configPath();
    if (dir.empty() || path.empty()) {
        return false;
    }

    CreateDirectoryW(dir.c_str(), nullptr);

    std::wostringstream json;
    json << L"{"
         << L"\"cross_type\":\"" << typeToString(this->type) << L"\","
         << L"\"color\":\"" << colorToHex(this->color) << L"\","
         << L"\"thickness\":" << this->thickness << L","
         << L"\"length\":" << this->length << L","
         << L"\"custom_scale\":" << this->customScale << L","
         << L"\"custom_png_path\":\"" << escapeJson(this->customPngPath) << L"\""
         << L"}";

    std::ofstream file(narrow(path), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const std::string utf8 = narrow(json.str());
    file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(file);
}
