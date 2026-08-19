#ifndef BSP_4OLEDS_H
#define BSP_4OLEDS_H


#include <stdint.h>
#include "bsp/bsp_oled_12864.h"

#ifdef __cplusplus
extern "C" {
#endif

//四块128x64 OLED组合的虚拟分辨率
#define OLED4_WIDTH 256U
#define OLED4_HEIGHT 128U

#define OLED4_PANEL_WIDTH 128U
#define OLED4_PANEL_HEIGHT 64U

#define OLED4_PANEL_COUNT 4U

//虚拟屏幕128行像素，8像素为一个字节，一共16个page
#define OLED_PAGE_COUNT (OLED4_HEIGHT / 8U)

//初始化4屏集成显示驱动
void BSP_4OLEDs_Init(void);
//清空虚拟帧缓冲，只修改RAM，还需执行BSP_4OLEDs_Refresh()
void BSP_4OLEDs_Clear(void);
//刷新虚拟帧缓冲到4块屏幕上
void BSP_4OLEDs_Refresh(void);


void BSP_4OLEDs_DrawPoint(uint16_t x,uint16_t y,uint8_t fill);
void BSP_4OLEDs_DrawLine(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1);
void BSP_4OLEDs_ShowImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *image);
void BSP_4OLEDs_ShowChar(uint16_t x,uint16_t y,char ch,oled_font_t font);
void BSP_4OLEDs_ShowString(uint16_t x,uint16_t y,const char *str,oled_font_t font);
void BSP_4OLEDs_ShowNumber(uint16_t x,uint16_t y,uint32_t num,oled_font_t font);

#ifdef __cplusplus
}
#endif

#endif
