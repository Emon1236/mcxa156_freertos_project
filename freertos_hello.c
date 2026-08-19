/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"

#include "bsp/bsp_spi.h"
#include "bsp/bsp_4oleds.h"




int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();

    BSP_SPI_Init();
    BSP_4OLEDs_Init();

    BSP_4OLEDs_Clear();

BSP_4OLEDs_ShowString(
    0U, 0U,
    "PANEL 0",
    OLED_FONT_8X16);

BSP_4OLEDs_ShowString(
    128U, 0U,
    "PANEL 1",
    OLED_FONT_8X16);

BSP_4OLEDs_ShowString(
    0U, 64U,
    "PANEL 2",
    OLED_FONT_8X16);

BSP_4OLEDs_ShowString(
    128U, 64U,
    "PANEL 3",
    OLED_FONT_8X16);

/* 绘制虚拟屏幕外框 */
BSP_4OLEDs_DrawLine(0U,   0U,   255U, 0U);
BSP_4OLEDs_DrawLine(255U, 0U,   255U, 127U);
BSP_4OLEDs_DrawLine(255U, 127U, 0U,   127U);
BSP_4OLEDs_DrawLine(0U,   127U, 0U,   0U);

/* 绘制四屏分界线 */
BSP_4OLEDs_DrawLine(127U, 0U, 127U, 127U);
BSP_4OLEDs_DrawLine(0U, 127U, 255U, 0U);

BSP_4OLEDs_Refresh();
    vTaskStartScheduler();
    for(;;);
}

