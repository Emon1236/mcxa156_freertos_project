#ifndef APP_BUZZER_TASK_H
#define APP_BUZZER_TASK_H

#include <stdbool.h>
#include <stdint.h>

bool APP_Buzzer_CreateTask(void);

/* 仅供任务上下文调用；提交请求后立即返回。 */
bool APP_Buzzer_PlayTone(uint32_t frequencyHz, uint32_t durationMs);

/* 异步提交停止请求。 */
bool APP_Buzzer_Stop(void);

#endif