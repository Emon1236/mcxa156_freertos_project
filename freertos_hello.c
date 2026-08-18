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

#include "bsp/bsp_rgb_led.h"
#include "app/app_rgb_task.h"

#include "bsp/bsp_spi.h"
#include "bsp/bsp_oled_12864.h"
#include "bsp/bsp_oled_data.h"
#include <stdint.h>


int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();

      

    vTaskStartScheduler();
    for(;;);
}

