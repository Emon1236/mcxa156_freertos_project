#include "app/app_buzzer_task.h"
#include "bsp/bsp_buzzer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define APP_BUZZER_MAX_DURATION_MS 10000U

typedef struct{
    uint32_t frequencyHz;
    uint32_t durationMs;
} buzzer_request_t;

//队列变量
static QueueHandle_t s_requestQueue = NULL;

static void app_buzzer_task(void *parameter){
    buzzer_request_t request;
    TickType_t waitTicks = portMAX_DELAY;

    (void)parameter;

    for (;;){
        /*
         * 空闲时等待请求。
         * 发声时等待新请求，最多等到当前音的结束时间。
         * 阻塞期间不占用 CPU，PWM 仍持续输出。
         */
        if (xQueueReceive(s_requestQueue, &request, waitTicks) == pdPASS){
            BSP_Buzzer_Stop();
            waitTicks = portMAX_DELAY;

            /* durationMs 为 0 表示停止。 */
            if (request.durationMs == 0U){
                continue;
            }

            if (BSP_Buzzer_Start(request.frequencyHz) ==
                kStatus_Success){
                /* 毫秒转 Tick，向上取整。 */
                waitTicks = (TickType_t)(
                    ((uint64_t)request.durationMs *
                     configTICK_RATE_HZ + 999U) / 1000U);
            }
        }
        else{
            /* 等待超时：当前提示音结束。 */
            BSP_Buzzer_Stop();
            waitTicks = portMAX_DELAY;
        }
    }
}

bool APP_Buzzer_CreateTask(void){
    if (s_requestQueue != NULL){
        return true;
    }

    if (BSP_Buzzer_Init() != kStatus_Success){
        return false;
    }

    /* 长度必须是 1，才能使用 xQueueOverwrite。 */
    s_requestQueue = xQueueCreate(1U, sizeof(buzzer_request_t));
    if (s_requestQueue == NULL){
        return false;
    }

    if (xTaskCreate(app_buzzer_task,
                    "buzzer",
                    configMINIMAL_STACK_SIZE + 128U,
                    NULL,
                    tskIDLE_PRIORITY + 2U,
                    NULL) != pdPASS)
    {
        vQueueDelete(s_requestQueue);
        s_requestQueue = NULL;
        return false;
    }

    return true;
}

bool APP_Buzzer_PlayTone(uint32_t frequencyHz, uint32_t durationMs){
    buzzer_request_t request;
    uint64_t ticks;

    if ((s_requestQueue == NULL) ||
        (frequencyHz < BSP_BUZZER_MIN_FREQUENCY_HZ) ||
        (frequencyHz > BSP_BUZZER_MAX_FREQUENCY_HZ) ||
        (durationMs == 0U) ||
        (durationMs > APP_BUZZER_MAX_DURATION_MS))
    {
        return false;
    }

    ticks = ((uint64_t)durationMs * configTICK_RATE_HZ + 999U)
            / 1000U;

    /* 避免转换溢出或误用无限等待值。 */
    if ((ticks == 0U) || (ticks >= (uint64_t)portMAX_DELAY)){
        return false;
    }

    request.frequencyHz = frequencyHz;
    request.durationMs = durationMs;

    return xQueueOverwrite(s_requestQueue, &request) == pdPASS;
}

bool APP_Buzzer_Stop(void){
    const buzzer_request_t request = {0U, 0U};

    if (s_requestQueue == NULL){
        return false;
    }

    return xQueueOverwrite(s_requestQueue, &request) == pdPASS;
}