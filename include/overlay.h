#pragma once

#include <windows.h>
#include <gdiplus.h>

#include "crosshair.h"

class Overlay {
public:
    Overlay();
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool create(HINSTANCE instance, const CrosshairSettings* settings, int width, int height);
    void destroy();
    bool isActive() const;
    void setSize(int width, int height);
    void refresh();
    void reloadImage();

private:
    HWND hWnd = nullptr;
    HINSTANCE hInstance = nullptr;
    const CrosshairSettings* settings = nullptr;
    Gdiplus::Image* customImage = nullptr;
    int width = 0;
    int height = 0;
    UINT_PTR timerId = 1;

    static Overlay* getObject(HWND hwnd);
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    void registerClass();
    void paint(HDC hdc);
    void clearImage();
};
