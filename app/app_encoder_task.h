#ifndef APP_ENCODER_TASK_H
#define APP_ENCODER_TASK_H

#include <stdbool.h>
#include <stdint.h>

#define APP_ENCODER_PERIOD_MS 1U
#define APP_ENCODER_TF_MS     25U
//控制快照每次更新，OLED队列每10次采样发送一次。
#define APP_ENCODER_UI_DIV    10U

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    float rawRpm;
    float filteredRpm;
    uint16_t rawAngle;

    uint32_t sequence;
    uint32_t timestampTicks;
    bool valid;
} app_encoder_sample_t;

bool APP_Encoder_CreateTask(void);
bool APP_Encoder_TryReceive(app_encoder_sample_t *sample);
/* 控制任务读取快照，不消耗UI队列中的数据。 */
bool APP_Encoder_GetLatest(app_encoder_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif
