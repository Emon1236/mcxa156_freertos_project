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
#include "bsp/bsp_oled_data.h"




int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();

    BSP_SPI_Init();
    BSP_4OLEDs_Init();

    BSP_4OLEDs_Clear();

    BSP_4OLEDs_Refresh();
    vTaskStartScheduler();
    for(;;);
}

