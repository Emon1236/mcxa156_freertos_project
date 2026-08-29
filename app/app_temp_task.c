/*
FreeRTOS Queue:操作系统队列，解决温度任务和UI任务之间的数据传递
UI内的256字节环形缓冲队列：保存最近约51s的历史记录
环形缓冲队列不是为了消除“丢帧问题”，他的主要价值是
写入新数据为O(1)，不需要每次移动255个元素；缓冲填满后自动覆盖最旧的数据
不会把未填充位置错误地理解为0，UI阻塞造成任务间数据积压由Queue缓冲
*/
#include "app/app_temp_task.h"
#include "bsp/bsp_p3t1755.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stddef.h>
#include <stdint.h>

//5Hz采样，每200ms采集一次温度。温度传感器默认转换时间为55ms，不会频繁读取重复结果
#define APP_TEMP_SAMPLE_PERIOD_MS 200U
//传感器初始化失败后重启间隔
#define APP_TEMP_INIT_RETRY_MS 1000U

/*
UI任务运行周期20ms，温度生产周期200ms
正常情况下队列中通常只有0或1条消息
队列长度8可以吸收UI短时间被蜂鸣器等功能阻塞的情况
*/
#define APP_TEMP_QUEUE_LENGTH 8U
#define APP_TEMP_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE + 128U)
#define APP_TEMP_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)

static QueueHandle_t s_tempQueue;

static void app_temp_task(void *parameter);

//将消息放入队列，队列满时丢弃最旧的一条，再保存最新的消息
static void app_temp_send_sample(const app_temp_sample_t *sample){
    app_temp_sample_t discarded;
    if(xQueueSend(s_tempQueue, sample, 0U) != pdPASS){
        //UI长时间没有消费消息时，队列可能已满，先丢弃旧消息，再写入新消息
        (void)xQueueReceive(s_tempQueue, &discarded, 0U);
        (void)xQueueSend(s_tempQueue, sample, 0U);
    }
}
//创建温度采集任务及其消息队列
bool APP_Temp_CreateTask(void){
    BaseType_t result;

    s_tempQueue = xQueueCreate(APP_TEMP_QUEUE_LENGTH, sizeof(app_temp_sample_t));
    if(s_tempQueue == NULL){
        return false;
    }
    result = xTaskCreate(app_temp_task, "temp", APP_TEMP_TASK_STACK_DEPTH, NULL, APP_TEMP_TASK_PRIORITY, NULL);
    if(result != pdPASS){
        vQueueDelete(s_tempQueue);
        s_tempQueue = NULL;
        return false;
    }
    return true;
}
//尝试从队列中接收温度采样数据，若队列为空或参数无效则返回false
bool APP_Temp_TryReceive(app_temp_sample_t *sample){
    if ((s_tempQueue == NULL) || (sample == NULL)){
        return false;
    }
    return xQueueReceive(s_tempQueue, sample, 0U) == pdPASS;
}

//
static void app_temp_task(void *parameter){
    TickType_t lastWakeTime;
    app_temp_sample_t sample;
    uint32_t sequence = 0U;
    status_t status;

    (void)parameter;

    for(;;){
        status = BSP_P3T1755_Init();
        if(status == kStatus_Success){
            break;
        }
        sample.temperatureQ4 = 0U; // 初始化为0，等待下一次采样
        sample.valid = false;
        sample.sequence = sequence++;

        app_temp_send_sample(&sample);
        vTaskDelay(pdMS_TO_TICKS(APP_TEMP_INIT_RETRY_MS));
    }
    lastWakeTime = xTaskGetTickCount();
    for(;;){
        int16_t temperatureQ4;
        status = BSP_P3T1755_ReadTemperatureQ4(&temperatureQ4);
        sample.sequence = sequence++;
        if(status == kStatus_Success){
            sample.temperatureQ4 = temperatureQ4;
            sample.valid = true;
        }else{
            //读取失败发送valid = false
            sample.temperatureQ4 = 0U;
            sample.valid = false;
        }
        app_temp_send_sample(&sample);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_TEMP_SAMPLE_PERIOD_MS));
    }
}

UBaseType_t APP_Temp_GetPendingSampleCount(void){
    if(s_tempQueue == NULL){
        return 0U;
    }
    return uxQueueMessagesWaiting(s_tempQueue);
}

