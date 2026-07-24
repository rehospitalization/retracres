#pragma once

#include <string>
#include <windows.h>

class DisplayMode {
public:
    bool captureCurrent();
    bool apply(int width, int height);
    bool restore();
    bool hasBackup() const;
    bool modeAvailable(int width, int height) const;

    int backupWidth() const;
    int backupHeight() const;

    bool loadBackup();
    bool saveBackup() const;

private:
    DEVMODEW backup = {};
    bool backupValid = false;

    static std::wstring backupPath();
};
