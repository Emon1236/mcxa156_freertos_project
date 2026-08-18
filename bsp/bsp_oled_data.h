#ifndef BSP_OLED_DATA_H
#define BSP_OLED_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 6x8 ASCII字模：仅收录 "test" 用到的字符(t/e/s)以及数字0-9(供显示数字功能使用)、空格 */
typedef struct{
    char ch;
    uint8_t data[6]; /* 每列1字节，共6列，纵向8行 */
} ascii_font_6x8_t;

extern const ascii_font_6x8_t g_asciiFont6x8[];
extern const uint8_t g_asciiFont6x8Count;

/* 16x16中文字模：仅收录 "测试" 二字，按需通过下标索引使用 */
typedef struct{
    uint8_t data[32]; /* 16x16点阵，每行2字节，共16行 */
} chinese_font_16x16_t;

extern const chinese_font_16x16_t g_chineseFont16x16[];

#define OLED_FONT_INDEX_CE  0U /* "测" */
#define OLED_FONT_INDEX_SHI 1U /* "试" */

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_DATA_H */