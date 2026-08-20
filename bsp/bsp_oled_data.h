#ifndef BSP_OLED_DATA_H
#define BSP_OLED_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/*ASCII字模数据声明*/
#define OLED_ASCII_FIRST_CHAR ' '
#define OLED_ASCII_LAST_CHAR  '~'
#define OLED_ASCII_CHAR_COUNT (OLED_ASCII_LAST_CHAR - OLED_ASCII_FIRST_CHAR + 1U)

extern const uint8_t OLED_F6X8[];
extern const uint8_t OLED_F8X16[];

// 28x28菜单图标参数
#define OLED_ICON_WIDTH  28U
#define OLED_ICON_HEIGHT 28U

#define OLED_ICON_28X28_DATA_SIZE (OLED_ICON_WIDTH * ((OLED_ICON_HEIGHT + 7U) / 8U))
//28x28图标编号
typedef enum{
    OLED_ICON_SETTINGS = 0U,
    OLED_ICON_TEMP_SENSOR,
    OLED_ICON_IMG_TRANSMISSION,
    OLED_ICON_CLOCK,
    OLED_ICON_GAME,
    OLED_ICON_WEATHER,
    OLED_ICON_MUSIC,
    OLED_ICON_WIFI,
    OLED_ICON_CTRL_PANEL,
    OLED_ICON_3D,
    OLED_ICON_COUNT
} oled_icon_id_t;
//声明28x28的图标库
extern const uint8_t g_oledIcons28x28[OLED_ICON_COUNT][OLED_ICON_28X28_DATA_SIZE];

#define OLED_FONT_INDEX_CE  0U /* "测" */
#define OLED_FONT_INDEX_SHI 1U /* "试" */

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_DATA_H */