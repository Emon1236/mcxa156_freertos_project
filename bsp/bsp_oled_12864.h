#ifndef BSP_OLED_12864_H
#define BSP_OLED_12864_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH  128U
#define OLED_HEIGHT 64U
#define OLED_PANEL_COUNT 4U

typedef enum{
    OLED_FONT_6X8 = 0,
    OLED_FONT_8X16
} oled_font_t;

#define  OLED_FONT_WIDTH(font)  ((font) == OLED_FONT_6X8 ? 6U : 8U)
#define  OLED_FONT_HEIGHT(font) ((font) == OLED_FONT_6X8 ? 8U : 16U)

/* 通过A0/A1选中0~3号屏幕(对应译码器Y0~Y3)，之后的写命令/写数据只会被选中的屏幕接收 */
void BSP_OLED_SelectPanel(uint8_t index);

void BSP_OLED_Init(uint8_t index);  //OLED初始化，并选择初始化的屏幕，index0~3
void BSP_OLED_Clear(void);          //清屏函数
void BSP_OLED_Refresh(void);        //刷新显示函数，将缓冲区数据写入屏幕

void BSP_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t fill);            //画点函数，x:0~127，y:0~63，fill:1点亮/0熄灭
void BSP_OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);  //画线函数，x0,y0:起点坐标，x1,y1:终点坐标

void BSP_OLED_ShowImage(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *image); //显示图像函数，x:0~127，y:0~63，width:图像宽度，height:图像高度，image:图像数据指针

void BSP_OLED_ShowChar(uint8_t x, uint8_t y, char ch, oled_font_t font);                  //显示字符函数，x:0~127，y:0~63，ch:要显示的字符，font:字体类型
void BSP_OLED_ShowString(uint8_t x, uint8_t y, const char *str, oled_font_t font);        //显示字符串函数，x:0~127，y:0~63，str:要显示的字符串，font:字体类型
void BSP_OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num, oled_font_t font);           //显示数字函数，x:0~127，y:0~63，num:要显示的数字，font:字体类型

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_12864_H */