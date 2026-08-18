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

#define OLED_FONT_INDEX_CE  0U /* "测" */
#define OLED_FONT_INDEX_SHI 1U /* "试" */

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_DATA_H */