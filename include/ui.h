#pragma once

#include <string>
#include <windows.h>
#include <commctrl.h>

#include "color.h"
#include "configuration.h"
#include "crosshair.h"
#include "draw.h"
#include "resources.h"

class UserInterface {
public:
    UserInterface(HWND hwnd, HINSTANCE hInstance);
    ~UserInterface();

    void initialize();
    void fromConfiguration(Configuration*);
    void fromCrosshair(const CrosshairSettings&);
    void syncCrosshairTo(CrosshairSettings&) const;
    void setFonts(HFONT body, HFONT small, HFONT title, HFONT button);
    void applyPreset(int width, int height);
    void setWindowMode(int controlId);
    void updateModeHint();
    void updateCrosshairHint();
    void toggleReadOnly();
    int getSelectedWindowMode() const;
    bool isReadOnlyLocked() const;
    bool getResolution(int* width, int* height, int* fps) const;
    bool getOverlaySize(int* width, int* height) const;
    void setFocusedEdit(HWND edit);
    void setHoveredControl(int controlId);
    void onAnimTick();
    void paint(HDC hdc);
    void paintResolutionPage(HDC hdc);
    void paintCrosshairPage(HDC hdc);
    void onHostResize();
    void fitWindowToContent();
    void selectTab(int index);
    void onTabChanged();
    void setCrossType(CrossType type);
    void setCrossColor(COLORREF color);
    void onTrackbar(HWND trackbar);
    void paintTrackbar(HWND trackbar, HDC hdc);
    void setCustomPngPath(const std::wstring& path);
    void clearCustomPng();
    void updateStandardControlsEnabled();
    void syncActivePreset();
    CrossType getCrossType() const;
    COLORREF getCrossColor() const;
    int getCrossThickness() const;
    int getCrossLength() const;
    int getCustomScale() const;
    const std::wstring& getCustomPngPath() const;

    void createControl();
    void createBrush();

    LRESULT onColorEdit(WPARAM wParam);
    LRESULT onColorStatic(WPARAM wParam, LPARAM lParam);
    LRESULT onDraw(WPARAM, LPARAM);

private:
    HWND hWnd = nullptr;
    HINSTANCE hInstance = nullptr;

    HFONT hFont = nullptr;
    HFONT hFontSmall = nullptr;
    HFONT hFontTitle = nullptr;
    HFONT hFontButton = nullptr;

    HBRUSH hBrushStatic = nullptr;
    HBRUSH hBrushEdit = nullptr;

    HWND tab_resolution = nullptr;
    HWND tab_crosshair = nullptr;
    HWND page_resolution = nullptr;
    HWND page_crosshair = nullptr;

    HWND apply_button = nullptr;
    HWND revert_button = nullptr;
    HWND readonly_checkbox = nullptr;
    HWND width_edit = nullptr;
    HWND height_edit = nullptr;
    HWND fps_edit = nullptr;
    HWND width_label = nullptr;
    HWND height_label = nullptr;
    HWND fps_label = nullptr;
    HWND display_label = nullptr;
    HWND presets_label = nullptr;
    HWND mode_label = nullptr;
    HWND preset_1440 = nullptr;
    HWND preset_1560 = nullptr;
    HWND preset_1550 = nullptr;
    HWND fullscreen_radio = nullptr;
    HWND windowed_fullscreen_radio = nullptr;
    HWND windowed_radio = nullptr;
    HWND mode_hint = nullptr;

    HWND cross_type_label = nullptr;
    HWND cross_plus = nullptr;
    HWND cross_x = nullptr;
    HWND cross_dot = nullptr;
    HWND cross_circle = nullptr;
    HWND cross_load_png = nullptr;
    HWND cross_clear_png = nullptr;
    HWND cross_scale_label = nullptr;
    HWND cross_scale = nullptr;
    HWND cross_color_label = nullptr;
    HWND cross_color_button = nullptr;
    HWND cross_thick_label = nullptr;
    HWND cross_thick = nullptr;
    HWND cross_length_label = nullptr;
    HWND cross_length = nullptr;
    HWND cross_hint = nullptr;
    HWND cross_start = nullptr;
    HWND cross_stop = nullptr;

    HWND focused_edit = nullptr;
    int hovered_control = 0;
    int fading_control = 0;
    float hover_t = 0.0f;
    float fade_t = 0.0f;
    bool anim_running = false;

    int active_preset_width = 0;
    int active_preset_height = 0;
    int selected_mode_id = IDC_FS_RADIO;
    bool read_only_locked = true;
    int selected_tab = 0;

    CrossType cross_type = CrossType::Plus;
    COLORREF cross_color_value = RGB(0xF5, 0x7C, 0x00);
    std::wstring custom_png_path;
    std::wstring mode_hint_text;
    std::wstring cross_hint_text;

    void createTabs();
    void createPages();
    void layoutPages();
    void createResolutionControls();
    void createCrosshairControls();
    void createApplyButton();
    void createRevertButton();
    void createCheckbox();
    void createEdit();
    void createLabel();
    void createPresets();
    void createRadio();
    void layoutModeHint();
    void layoutCrosshairHint();
    int measureHintHeight(const std::wstring& text) const;
    void subclassEdit(HWND edit);
    void subclassInteractive(HWND control);
    void subclassPanel(HWND panel);
    void subclassTrackbar(HWND trackbar);
    void centerEditText(HWND edit);
    void drawInputChrome(HDC hdc, HWND parent, HWND edit);
    void drawTipFromControl(Gdiplus::Graphics& g, HWND control, const std::wstring& text);
    void drawSectionFromLabel(
        Gdiplus::Graphics& g,
        HWND label,
        Draw::Icon icon,
        const wchar_t* title
    );
    void drawChip(LPDRAWITEMSTRUCT, int width, int height, const wchar_t* label);
    void drawPrimaryButton(LPDRAWITEMSTRUCT, int controlId, const wchar_t* label);
    void drawSecondaryButton(LPDRAWITEMSTRUCT, int controlId, const wchar_t* label);
    void drawRadio(LPDRAWITEMSTRUCT, int selectedId, const wchar_t* label);
    void drawCheckbox(LPDRAWITEMSTRUCT, const wchar_t* label);
    void drawColorSwatch(LPDRAWITEMSTRUCT);
    void drawSegmentTab(LPDRAWITEMSTRUCT, int tabIndex, const wchar_t* label);
    void drawTypeSegment(LPDRAWITEMSTRUCT, int index, int count, CrossType type, const wchar_t* label);
    void invalidateControl(HWND control);
    void invalidateCrossTypeControls();
    void invalidateTabs();
    void invalidateInteractive();
    void styleTrackbar(HWND trackbar);
    void startHoverAnim();
    float pressAmount(LPDRAWITEMSTRUCT pdis) const;
    float hoverAmount(int controlId) const;
};
