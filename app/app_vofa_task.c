#include "app/app_vofa_task.h"
#include "app/app_encoder_task.h"
#include "bsp/bsp_uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <string.h>

#define APP_VOFA_SAMPLE_TIMEOUT_MS 20U

static void app_vofa_task(void *parameter);

static void app_vofa_send_rpm(float rawRpm, float filteredRpm){
    float data[2];
    uint8_t frame[12];

    data[0] = rawRpm;      //VOFA通道0：原始速度，RPM
    data[1] = filteredRpm; //VOFA通道1：低通滤波速度，RPM

    //A156的float为4字节、小端；前8字节为两路float。
    memcpy(frame, data, sizeof(data));
    //JustFloat固定帧尾，按二进制发送，不使用字符串长度。
    frame[8] = 0x00U;
    frame[9] = 0x00U;
    frame[10] = 0x80U;
    frame[11] = 0x7FU;

    (void)BSP_UART_Send(frame, sizeof(frame));
}

bool APP_VOFA_CreateTask(void){
    if (BSP_UART_Init(APP_VOFA_BAUDRATE) != kStatus_Success){
        return false;
    }

    return xTaskCreate(app_vofa_task, "vofa",
                       configMINIMAL_STACK_SIZE + 128U, NULL,
                       tskIDLE_PRIORITY + 1U, NULL) == pdPASS;
}

static void app_vofa_task(void *parameter){
    TickType_t lastWakeTime = xTaskGetTickCount();
    (void)parameter;

    for (;;){
        app_encoder_sample_t sample;

        //读取快照，不取走OLED队列的数据，也不再次读取磁编码器。
        if (APP_Encoder_GetLatest(&sample) && sample.valid &&
            (TickType_t)(xTaskGetTickCount() - sample.timestampTicks) <=
                pdMS_TO_TICKS(APP_VOFA_SAMPLE_TIMEOUT_MS)){
            app_vofa_send_rpm(sample.rawRpm, sample.filteredRpm);
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_VOFA_PERIOD_MS));
    }
}
