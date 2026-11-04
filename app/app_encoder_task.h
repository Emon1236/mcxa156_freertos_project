#ifndef APP_ENCODER_TASK_H
#define APP_ENCODER_TASK_H

#include <stdbool.h>
#include <stdint.h>

#define APP_ENCODER_PERIOD_MS 10U
#define APP_ENCODER_TF_MS     50U

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    float rawRpm;
    float filteredRpm;

    uint32_t sequence;
    uint32_t timestampTicks;
    bool valid;
} app_encoder_sample_t;

bool APP_Encoder_CreateTask(void);
bool APP_Encoder_TryReceive(app_encoder_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif