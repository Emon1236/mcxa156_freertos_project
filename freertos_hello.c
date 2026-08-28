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

#include "app/app_ui_task.h"
#include "app/app_temp_task.h"


int main(void)
{

    BOARD_InitHardware();
    if (!APP_Temp_CreateTask())
    {
        PRINTF("Failed to create temperature task.\r\n");
        for (;;)
        {
        }
    }    

    if (!APP_UI_CreateTask())
    {
        /*
         * 任务创建失败一般表示FreeRTOS堆空间不足。
         * 此处停机，方便调试器定位。
         */
        for (;;)
        {
        }
    }
  

    vTaskStartScheduler();

    for (;;)
    {
    }
}
