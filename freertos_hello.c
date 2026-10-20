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
#include "app/app_buzzer_task.h"
#include "FOC/FOC.h"
#include "FOC/AS5600.h"
#include "app/app_motor_task.h"
#include "bsp/bsp_i3c.h"


int main(void)
{
    status_t status;
    BOARD_InitHardware();
    status = BSP_I3C_Init();
    if (status != kStatus_Success){
        PRINTF("I3C init failed: %d\r\n", (int)status);
        for (;;){}
    }
    if (!APP_Temp_CreateTask())
    {
        PRINTF("Failed to create temperature task.\r\n");
        for (;;)
        {
        }
    }

    PRINTF("System boot.\r\n");

    status = FOC_Init(12.6f);
    PRINTF("FOC_Init returned: %d\r\n", (int)status);

    if (status != kStatus_Success)
    {
        for (;;)
        {
        }
    }

    if (!Foc_position_Createdtask())
    {
        PRINTF("Failed to create FOC task.\r\n");

        for (;;)
        {
        }
    }
    if (!APP_Buzzer_CreateTask()){
        PRINTF("Failed to create buzzer task.\r\n");
        for (;;){}
    }

    if (!APP_UI_CreateTask()){
        PRINTF("Failed to create UI task.\r\n");

        for (;;)
        {
        }
    }
  

    PRINTF("Starting scheduler, tick rate=%u Hz.\r\n",
           (unsigned int)configTICK_RATE_HZ);

    vTaskStartScheduler();

    PRINTF("Scheduler returned unexpectedly.\r\n");

    for (;;)
    {
    }
}