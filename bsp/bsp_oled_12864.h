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

/* 通过A0/A1选中0~3号屏幕(对应译码器Y0~Y3)，之后的写命令/写数据只会被选中的屏幕接收 */
void BSP_OLED_SelectPanel(uint8_t index);

void BSP_OLED_Init(uint8_t index);  //OLED初始化，并选择初始化的屏幕，index0~3
void BSP_OLED_Clear(void);          //清屏函数
void BSP_OLED_Refresh(void);        //刷新显示函数，将缓冲区数据写入屏幕

void BSP_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t fill);            //画点函数，x:0~127，y:0~63，fill:1点亮/0熄灭
void BSP_OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);  //画线函数，x0,y0:起点坐标，x1,y1:终点坐标

void BSP_OLED_ShowChar(uint8_t x, uint8_t y, char ch);                  //显示字符函数，x:0~127，y:0~63，ch:要显示的字符
void BSP_OLED_ShowString(uint8_t x, uint8_t y, const char *str);        //显示字符串函数，x:0~127，y:0~63，str:要显示的字符串
void BSP_OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num);           //显示数字函数，x:0~127，y:0~63，num:要显示的数字
void BSP_OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t fontIndex);     //显示中文函数，x:0~127，y:0~63，fontIndex:要显示的中文索引

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_12864_H */