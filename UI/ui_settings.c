#include "UI/ui_settings.h"

/*
 * 用户设置默认值:
 * 初始字体：6×8
 * 初始按键音：开启
 * 初始光标：反色
 */
static app_settings_t s_settings = {
    .font = APP_FONT_SMALL,
    .keySoundEnabled = true,
    .cursorStyle = APP_CURSOR_INVERSE,
    .displayBrightness = 255U,

    .tileAnimationMs = 110U,
    .listAnimationMs = 100U,
    .windowAnimationMs = 90U,
    .fadeStepMs = 30U,

    .tileUnfoldEnabled = true,
    .listUnfoldEnabled = true,
    .tileLoopEnabled = true,
    .listLoopEnabled = true,
    .windowBokehEnabled = false,
    .darkModeEnabled = true
};

void APP_Settings_Init(void){
    s_settings.font = APP_FONT_SMALL;
    s_settings.keySoundEnabled = true;
    s_settings.cursorStyle = APP_CURSOR_INVERSE;    
    s_settings.displayBrightness = 255U;
    
    s_settings.tileAnimationMs = 110U;
    s_settings.listAnimationMs = 100U;
    s_settings.windowAnimationMs = 90U;
    s_settings.fadeStepMs = 30U;

    s_settings.tileUnfoldEnabled = true;
    s_settings.listUnfoldEnabled = true;
    s_settings.tileLoopEnabled = true;
    s_settings.listLoopEnabled = true;
    s_settings.windowBokehEnabled = false;
    s_settings.darkModeEnabled = true;
}
//获取当前设置，只允许读，不允许外部修改
const app_settings_t *APP_Settings_Get(void){
    return &s_settings;
}
//在small和large之间切换字体
void APP_Settings_NextFont(void){
    if (s_settings.font == APP_FONT_SMALL){
        s_settings.font = APP_FONT_LARGE;
    }
    else{
        s_settings.font = APP_FONT_SMALL;
    }
}
//切换按键音开关
void APP_Settings_ToggleKeySound(void){
    s_settings.keySoundEnabled = !s_settings.keySoundEnabled;
}
//切换光标样式
void APP_Settings_NextCursorStyle(void){
    if (s_settings.cursorStyle == APP_CURSOR_INVERSE){
        s_settings.cursorStyle = APP_CURSOR_BORDER;
    }
    else{
        s_settings.cursorStyle = APP_CURSOR_INVERSE;
    }
}
//OLED显示亮度
void APP_Settings_SetBrightness(uint8_t brightness){
    s_settings.displayBrightness = brightness;
}

void APP_Settings_SetTileAnimationMs(uint8_t value)
{
    s_settings.tileAnimationMs = value;
}

void APP_Settings_SetListAnimationMs(uint8_t value)
{
    s_settings.listAnimationMs = value;
}

void APP_Settings_SetWindowAnimationMs(uint8_t value)
{
    s_settings.windowAnimationMs = value;
}

void APP_Settings_SetFadeStepMs(uint8_t value)
{
    s_settings.fadeStepMs = value;
}

void APP_Settings_ToggleTileUnfold(void)
{
    s_settings.tileUnfoldEnabled =
        !s_settings.tileUnfoldEnabled;
}

void APP_Settings_ToggleListUnfold(void)
{
    s_settings.listUnfoldEnabled =
        !s_settings.listUnfoldEnabled;
}

void APP_Settings_ToggleTileLoop(void)
{
    s_settings.tileLoopEnabled =
        !s_settings.tileLoopEnabled;
}

void APP_Settings_ToggleListLoop(void)
{
    s_settings.listLoopEnabled =
        !s_settings.listLoopEnabled;
}

void APP_Settings_ToggleWindowBokeh(void)
{
    s_settings.windowBokehEnabled =
        !s_settings.windowBokehEnabled;
}

void APP_Settings_ToggleDarkMode(void)
{
    s_settings.darkModeEnabled =
        !s_settings.darkModeEnabled;
}
