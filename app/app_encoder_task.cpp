#include "app/app_encoder_task.h"
#include "FOC/AS5600.h"
#include "FOC/low_filter.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define ENCODER_QUEUE_LENGTH 32U

static QueueHandle_t s_encoderQueue;

static void encoder_task(void *parameter);

/* 队列满时保留更新的数据，sequence 用于识别缺失。 */
static void encoder_send_sample(const app_encoder_sample_t *sample){
    app_encoder_sample_t discarded;

    if (xQueueSend(s_encoderQueue, sample, 0U) != pdPASS){
        (void)xQueueReceive(s_encoderQueue, &discarded, 0U);
        (void)xQueueSend(s_encoderQueue, sample, 0U);
    }
}

bool APP_Encoder_CreateTask(void){
    s_encoderQueue = xQueueCreate(ENCODER_QUEUE_LENGTH,sizeof(app_encoder_sample_t));

    if (s_encoderQueue == NULL){
        return false;
    }

    if (xTaskCreate(encoder_task, "encoder",
                 configMINIMAL_STACK_SIZE + 256U, NULL, tskIDLE_PRIORITY + 3U, NULL) != pdPASS){
        vQueueDelete(s_encoderQueue);
        s_encoderQueue = NULL;
        return false;
    }

    return true;
}

bool APP_Encoder_TryReceive(app_encoder_sample_t *sample){
    if (s_encoderQueue == NULL || sample == NULL){
        return false;
    }

    return xQueueReceive(s_encoderQueue, sample, 0U) == pdPASS;
}

static void encoder_task(void *parameter){
    LowPassFilter filter(APP_ENCODER_TF_MS * 0.001f);

    uint32_t sequence = 0U;
    uint16_t previousRaw = 0U;
    TickType_t previousTick = 0U;
    bool hasPrevious = false;

    (void)parameter;

    /* 上电初始化；失败时后台重试，不阻塞 UI。 */
    for (;;){
        if (AS5600_Init() == kStatus_Success && AS5600_CheckMagnet() == kStatus_Success){
            break;
        }

        app_encoder_sample_t sample = {};
        sample.sequence = sequence++;
        sample.timestampTicks = (uint32_t)xTaskGetTickCount();
        encoder_send_sample(&sample);

        vTaskDelay(pdMS_TO_TICKS(1000U));
    }

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;){
        app_encoder_sample_t sample = {};
        uint16_t raw = 0U;

        status_t status = AS5600_ReadRawAngle(&raw);
        TickType_t now = xTaskGetTickCount();

        sample.sequence = sequence++;
        sample.timestampTicks = (uint32_t)now;

        if (status != kStatus_Success){
            hasPrevious = false;
            filter.Reset();
        }
        else if (!hasPrevious){
            /* 第一次读取只建立差分基准。 */
            previousRaw = raw;
            previousTick = now;
            hasPrevious = true;
        }
        else
        {
            TickType_t elapsed = (TickType_t)(now - previousTick);

            if (elapsed != 0U){
                float dt = (float)elapsed / (float)configTICK_RATE_HZ;

                if (dt <= 0.1f){
                    int32_t delta = (int32_t)raw - (int32_t)previousRaw;

                    /* 处理正反方向跨越零点。 */
                    if (delta > 2048){
                        delta -= 4096;
                    }
                    else if (delta < -2048){
                        delta += 4096;
                    }

                    sample.rawRpm = (float)delta * (60.0f / 4096.0f) / dt;

                    sample.filteredRpm = filter.Update(sample.rawRpm, dt);

                    sample.valid = true;
                }
                else{
                    /* 间隔过长，重新建立测速基准。 */
                    filter.Reset();
                }

                previousRaw = raw;
                previousTick = now;
            }
        }

        encoder_send_sample(&sample);

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }
}