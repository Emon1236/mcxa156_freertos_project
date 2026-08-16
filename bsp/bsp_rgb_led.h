/*
BSP层 RGB驱动头文件

额外说明，由于工程的CMakeLists中启用了project(freertos_project LANGUAGES C CXX ASM ...)
.h公共头文件建议添加extern "C" {}以便同时兼容C和C++。
*/

#ifndef BSP_RGB_LED_H
#define BSP_RGB_LED_H

#ifdef __cplusplus
extern "C" {
#endif

// 枚举类型定义，用于表示RGB LED颜色状态
typedef enum{
    BSP_RGB_COLOR_OFF = 0,
    BSP_RGB_COLOR_RED,
    BSP_RGB_COLOR_GREEN,
    BSP_RGB_COLOR_BLUE
} bsp_rgb_led_t;

//初始化板载RGB LED三个GPIO
void BSP_RGB_LED_Init(void);

//只允许红，绿，蓝其中一路点亮
void BSP_RGB_SetColor(bsp_rgb_led_t color);

// 关闭RGB LED
void BSP_RGB_LED_Off(void);

#ifdef __cplusplus
}
#endif   

#endif