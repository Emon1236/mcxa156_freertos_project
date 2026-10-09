#ifndef APP_TEMP_TASK_H
#define APP_TEMP_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"   // 包含FreeRTOS的基本类型定义，例如UBaseType_t
#include "queue.h"   // 包含FreeRTOS队列相关的类型定义，例如QueueHandle_t

#ifdef __cplusplus
extern "C" {
#endif
//温度任务发送给UI任务的消息
typedef struct{
    int16_t temperatureQ4;   // 温度值，Q4格式，单位为1/16摄氏度
    bool valid;   // 本次读取是否成功
    uint32_t sequence;   // 采样的序列号，主要用于调试
} app_temp_sample_t;

bool APP_Temp_CreateTask(void); //创建温度消息队列和5Hz采样任务
// 尝试从温度消息队列接收最新的温度样本，非阻塞
bool APP_Temp_TryReceive(app_temp_sample_t *sample); 

//测试接口，不在正式功能中出现
UBaseType_t APP_Temp_GetPendingSampleCount(void); // 获取队列中待处理的温度样本数量


#ifdef __cplusplus
}
#endif

#endif // APP_TEMP_TASK_H