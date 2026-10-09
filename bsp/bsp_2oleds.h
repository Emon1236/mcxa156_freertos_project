#ifndef BSP_2OLEDS_H
#define BSP_2OLEDS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 2、3号OLED在物理上左右排列：
 *
 * panel 2          panel 3
 * x = 0~127        x = 128~255
 *
 * 两块屏幕组成一个256×64逻辑显示区域。
 */
#define BSP_2OLEDS_WIDTH          256U
#define BSP_2OLEDS_HEIGHT         64U
#define BSP_2OLEDS_PAGE_COUNT       8U
#define BSP_2OLEDS_FRAME_BYTES      (BSP_2OLEDS_WIDTH * BSP_2OLEDS_HEIGHT / 8U)

//初始化2，3号OLED控制器
bool BSP_2OLEDS_Init(void);
//接收256x64单色帧缓冲并发送到2，3号屏幕
bool BSP_2OLEDS_Present(const uint8_t frameBuffer[BSP_2OLEDS_FRAME_BYTES]);
//清空2，3号屏幕
bool BSP_2OLEDS_Clear(void);


#ifdef __cplusplus
}
#endif

#endif /* BSP_2OLEDS_H */