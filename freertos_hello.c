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
#include "FOC/AS5600.h"

static void as5600_test_task(void *parameter)
{
    status_t status;
    uint16_t rawAngle;
    uint32_t angleMilliDegrees;

    (void)parameter;

    for (;;)
    {
        status = AS5600_ReadRawAngle(&rawAngle);

        if (status == kStatus_Success)
        {
            /*
             * 这里直接由已经读取的rawAngle计算角度，
             * 避免再调用一次读取函数、重复访问总线。
             */
            angleMilliDegrees =
                ((uint32_t)rawAngle * 360000U +
                 AS5600_COUNTS_PER_REVOLUTION / 2U) /
                AS5600_COUNTS_PER_REVOLUTION;

            PRINTF(
                "AS5600 raw=%u, angle=%u.%03u deg\r\n",
                (unsigned int)rawAngle,
                (unsigned int)(angleMilliDegrees / 1000U),
                (unsigned int)(angleMilliDegrees % 1000U));
        }
        else
        {
            PRINTF("AS5600 read failed: %d\r\n", (int)status);
        }

        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

int main(void)
{

    BOARD_InitHardware();
    AS5600_Init();
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
    if (xTaskCreate(as5600_test_task, "AS5600 Test Task", configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    {
        PRINTF("Failed to create AS5600 test task.\r\n");
        for (;;)
        {
        }
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}
