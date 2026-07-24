#pragma once

#ifdef APSTUDIO_INVOKED
    #ifndef APSTUDIO_READONLY_SYMBOLS
        #define _APS_NEXT_RESOURCE_VALUE        101
        #define _APS_NEXT_COMMAND_VALUE         40001
        #define _APS_NEXT_CONTROL_VALUE         1001
        #define _APS_NEXT_SYMED_VALUE           101
    #endif
#endif

#define IDR_ICON           2000

#define IDC_HEIGHT_LABEL   2005
#define IDC_WIDTH_LABEL    2010
#define IDC_FPS_LABEL      2015
#define IDC_DISPLAY_LABEL  2018

#define IDC_HEIGHT_EDIT    2020
#define IDC_WIDTH_EDIT     2025
#define IDC_FPS_EDIT       2030

#define IDC_MODE_LABEL     2035
#define IDC_FS_RADIO       2040
#define IDC_WFS_RADIO      2045
#define IDC_W_RADIO        2050
#define IDC_MODE_HINT      2052

#define IDC_RO_CHECKBOX    2055
#define IDC_APPLY_BUTTON   2060
#define IDC_REVERT_BUTTON  2065

#define IDC_PRESETS_LABEL  2070
#define IDC_PRESET_1440    2075
#define IDC_PRESET_1560    2080
#define IDC_PRESET_1550    2085

#define IDC_TAB_RESOLUTION 2100
#define IDC_TAB_CROSSHAIR  2101
#define IDC_PAGE_RESOLUTION 2105
#define IDC_PAGE_CROSSHAIR 2110

#define IDC_CROSS_TYPE_LABEL 2200
#define IDC_CROSS_PLUS       2205
#define IDC_CROSS_X          2210
#define IDC_CROSS_DOT        2215
#define IDC_CROSS_CIRCLE     2220

#define IDC_CROSS_LOAD_PNG   2225
#define IDC_CROSS_CLEAR_PNG  2230
#define IDC_CROSS_SCALE_LABEL 2235
#define IDC_CROSS_SCALE      2240

#define IDC_CROSS_COLOR_LABEL 2245
#define IDC_CROSS_COLOR      2250

#define IDC_CROSS_THICK_LABEL 2255
#define IDC_CROSS_THICK      2260

#define IDC_CROSS_LENGTH_LABEL 2265
#define IDC_CROSS_LENGTH     2270

#define IDC_CROSS_HINT       2275
#define IDC_CROSS_START      2280
#define IDC_CROSS_STOP       2285

#define IDC_TITLE_MIN        2300
#define IDC_TITLE_CLOSE      2301

namespace Chrome {
    inline constexpr int kTitleBarHeight = 44;
    inline constexpr int kTitleBtnSize = 32;
    inline constexpr int kTitleBtnGap = 6;
    inline constexpr int kCornerRadius = 12;
    // Keep content clear of the 1px frame ring.
    inline constexpr int kFrameInset = 1;
}
