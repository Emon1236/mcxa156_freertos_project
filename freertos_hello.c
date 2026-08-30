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
#include "FOC/FOC.h"

static void foc_openloop_test_task(void *parameter)
{
    TickType_t lastWakeTime;
    uint32_t printCounter;
    float targetMechanicalSpeed;
    float Uq;

    (void)parameter;

    /*
     * velocityopenloop()中的target单位是rad/s。
     *
     * 60RPM = 1转/秒 = 2π rad/s。
     *
     * 商家示例直接传入60，实际表示60rad/s，
     * 大约等于573RPM，而不是60RPM。
     */
    targetMechanicalSpeed = 6.28318530718f;

    lastWakeTime = xTaskGetTickCount();
    printCounter = 0U;

    for (;;)
    {
        Uq = velocityopenloop(
            targetMechanicalSpeed);

        /*
         * 每500次输出一次，避免PRINTF严重干扰1ms控制周期。
         */
        printCounter++;

        if (printCounter >= 500U)
        {
            printCounter = 0U;

            PRINTF(
                "OpenLoop: Uq=%d mV, "
                "angle=%d mrad, "
                "duty=%u/%u/%u\r\n",
                (int)(Uq * 1000.0f),
                (int)(shaft_angle * 1000.0f),
                (unsigned int)(dc_a * 1000.0f),
                (unsigned int)(dc_b * 1000.0f),
                (unsigned int)(dc_c * 1000.0f));
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(5U));
    }
}


int main(void)
{
    status_t status;

    BOARD_InitHardware();

    PRINTF("System boot.\r\n");

    status = FOC_Init(12.6f);

    PRINTF("FOC_Init returned: %d\r\n", (int)status);

    if (status != kStatus_Success)
    {
        for (;;)
        {
        }
    }

    if (xTaskCreate(foc_openloop_test_task,
                    "FOC OpenLoop",
                    configMINIMAL_STACK_SIZE + 256U,
                    NULL,
                    tskIDLE_PRIORITY + 3U,
                    NULL) != pdPASS)
    {
        PRINTF("Failed to create FOC task.\r\n");

        for (;;)
        {
        }
    }

    if (!APP_UI_CreateTask())
    {
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