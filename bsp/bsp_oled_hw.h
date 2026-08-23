#ifndef BSP_OLED_HW_H
#define BSP_OLED_HW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_OLED_PANEL_COUNT 4U
#define BSP_OLED_UI_PANEL    1U

/*
 * 只初始化OLED相关GPIO：
 * RESET、D/C、A0和A1。
 * SSD1315控制器初始化由U8g2负责。
 */
void BSP_OLED_HW_Init(void);

/* 通过译码器选择0～3号屏幕。 */
void BSP_OLED_HW_SelectPanel(uint8_t panel);

/* 控制四块OLED共用的RESET引脚。 */
void BSP_OLED_HW_SetReset(uint8_t level);

/* 0表示命令，1表示数据。 */
void BSP_OLED_HW_SetDataMode(uint8_t isData);

/* 通过现有BSP SPI驱动发送数据。 */
bool BSP_OLED_HW_Write(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_HW_H */