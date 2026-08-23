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
    .cursorStyle = APP_CURSOR_INVERSE
};

void APP_Settings_Init(void){
    s_settings.font = APP_FONT_SMALL;
    s_settings.keySoundEnabled = true;
    s_settings.cursorStyle = APP_CURSOR_INVERSE;    
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
