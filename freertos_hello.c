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

#define APP_TEMP_MONITOR_TASK_STACK_DEPTH  (configMINIMAL_STACK_SIZE + 160U)
#define APP_TEMP_MONITOR_TASK_PRIORITY     (tskIDLE_PRIORITY + 1U)
#define APP_TEMP_MONITOR_PERIOD_MS         1000U
/*
输出队列中的所有样本，显示读取状态，Q4原始值，摄氏温度以及读取前的队列样本数
每秒监视一次，采样周期是200ms，所以一次输出5行数据，队列的待处理数为5 / 8
*/
static void app_temp_monitor_task(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        app_temp_sample_t sample;
        UBaseType_t pendingSampleCount;

        /*
         * 读取前的数量表示这段时间温度生产任务累计写入、
         * 但尚未被本监视任务取走的样本数。
         */
        pendingSampleCount = APP_Temp_GetPendingSampleCount();

        PRINTF("Temp queue pending: %u / 8\r\n",
               (unsigned int)pendingSampleCount);

        while (APP_Temp_TryReceive(&sample))
        {
            if (sample.valid)
            {
                uint32_t magnitude;
                uint32_t integerPart;
                uint32_t fractionalPart;
                const char *sign;

                /*
                 * Q4 单位为 1/16 C。
                 * 例如 25.0625 C 对应 temperatureQ4 == 401。
                 */
                sign = "";
                if (sample.temperatureQ4 < 0)
                {
                    sign = "-";
                    magnitude = (uint32_t)(-(int32_t)sample.temperatureQ4);
                }
                else
                {
                    magnitude = (uint32_t)sample.temperatureQ4;
                }

                integerPart = magnitude / 16U;
                fractionalPart = (magnitude % 16U) * 625U;

                PRINTF("Temp sample #%u: %s%u.%04u C, Q4=%d\r\n",
                       (unsigned int)sample.sequence,
                       sign,
                       (unsigned int)integerPart,
                       (unsigned int)fractionalPart,
                       (int)sample.temperatureQ4);
            }
            else
            {
                PRINTF("Temp sample #%u: read failed\r\n",
                       (unsigned int)sample.sequence);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_TEMP_MONITOR_PERIOD_MS));
    }
}

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
    if (xTaskCreate(app_temp_monitor_task,
                    "temp_monitor",
                    APP_TEMP_MONITOR_TASK_STACK_DEPTH,
                    NULL,
                    APP_TEMP_MONITOR_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
        PRINTF("Failed to create temperature monitor task.\r\n");
        for (;;)
        {
        }
    }    

    vTaskStartScheduler();

    for (;;)
    {
    }
}
