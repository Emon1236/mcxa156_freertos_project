
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


int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();
    /* RGB初始化. */
    BSP_RGB_LED_Init();
    
    // if(xTaskCreate(APP_RGB_Task,"RGB_Task",configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1U,NULL) != pdPASS){
    //     PRINTF("RGB_Task create failed!\r\n");
    //     for(;;);
    // }
    if(xTaskCreate(APP_R_500ms_Task,"R_500ms_Task",configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1U,NULL) != pdPASS){
        PRINTF("R_500ms_Task create failed!\r\n");
        for(;;);
    }
    if(xTaskCreate(APP_G_2S_Task,"G_2S_Task",configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1U,NULL) != pdPASS){
        PRINTF("G_2S_Task create failed!\r\n");
        for(;;);
    }

    vTaskStartScheduler();
    for(;;);
}

