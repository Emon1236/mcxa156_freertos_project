#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
//应用层字体枚举
typedef enum{
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

    /* 各类动画响应时间，数值越小动画越快。 */
    uint8_t tileAnimationMs;
    uint8_t listAnimationMs;
    uint8_t windowAnimationMs;
    uint8_t fadeStepMs;    
    /* 参考UI中的开关配置。 */
    bool tileUnfoldEnabled;
    bool listUnfoldEnabled;
    bool tileLoopEnabled;
    bool listLoopEnabled;
    bool windowBokehEnabled;
    bool darkModeEnabled;
} app_settings_t;


void APP_Settings_Init(void);
const app_settings_t *APP_Settings_Get(void);
void APP_Settings_NextFont(void);
void APP_Settings_ToggleKeySound(void);
void APP_Settings_NextCursorStyle(void);
void APP_Settings_SetBrightness(uint8_t brightness);


void APP_Settings_SetTileAnimationMs(uint8_t value);
void APP_Settings_SetListAnimationMs(uint8_t value);
void APP_Settings_SetWindowAnimationMs(uint8_t value);
void APP_Settings_SetFadeStepMs(uint8_t value);
void APP_Settings_ToggleTileUnfold(void);
void APP_Settings_ToggleListUnfold(void);
void APP_Settings_ToggleTileLoop(void);
void APP_Settings_ToggleListLoop(void);
void APP_Settings_ToggleWindowBokeh(void);
void APP_Settings_ToggleDarkMode(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SETTINGS_H */