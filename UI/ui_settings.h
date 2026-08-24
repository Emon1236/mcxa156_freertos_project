#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
//应用层字体枚举
typedef enum
{
    APP_FONT_SMALL = 0U,
    APP_FONT_LARGE
} app_font_t;
//菜单光标样式
typedef enum{
    APP_CURSOR_INVERSE = 0U,    //反色
    APP_CURSOR_BORDER          //边框
}app_cursor_style_t;

typedef struct{
    app_font_t font;
    bool keySoundEnabled;
    app_cursor_style_t cursorStyle;
    uint8_t displayBrightness;  //OLED对比度，范围0~255
} app_settings_t;


void APP_Settings_Init(void);
const app_settings_t *APP_Settings_Get(void);
void APP_Settings_NextFont(void);
void APP_Settings_ToggleKeySound(void);
void APP_Settings_NextCursorStyle(void);
void APP_Settings_SetBrightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif /* APP_SETTINGS_H */