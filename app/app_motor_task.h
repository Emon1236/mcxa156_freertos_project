#ifndef APP_MOTOR_TASK_H
#define APP_MOTOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRECTION_POSITIVE ((int8_t)1)
#define DIRECTION_NEGATIVE ((int8_t)-1)

/* 与APP_Position_CreateTask二选一，同一组PWM只启动一个控制任务。 */
bool Foc_openloop_Createdtask(void);

void APP_Motor_SetDirection(int8_t direction);
int8_t APP_Motor_GetDirection(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_TASK_H */
