#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>

#include "bsp/bsp_oled_12864.h"

#ifdef __cplusplus
extern "C" {
#endif

//菜单光标样式
typedef enum{
    APP_CURSOR_INVERSE = 0U,    //反色
    APP_CURSOR_BORDER          //边框
}app_cursor_style_t;

//用户设置 （考虑掉电保存则将数据写入内部flash）
typedef struct{
    oled_font_t font;              //菜单字体
    bool keySoundEnabled;          //按键音效开关
    app_cursor_style_t cursorStyle; //菜单光标样式
}app_settings_t;

void APP_Settings_Init(void);
const app_settings_t *APP_Settings_Get(void);
void APP_Settings_NextFont(void);
void APP_Settings_ToggleKeySound(void);
void APP_Settings_NextCursorStyle(void);


#ifdef __cplusplus
}
#endif

#endif /* APP_SETTINGS_H */