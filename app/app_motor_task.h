#ifndef APP_MOTOR_TASK_H
#define APP_MOTOR_TASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRECTION_POSITIVE ((int8_t)1)
#define DIRECTION_NEGATIVE ((int8_t)-1)

typedef enum{
    APP_MOTOR_OFF = 0U,
    APP_MOTOR_OPENLOOP,
    APP_MOTOR_POSITION
}app_motor_mode_t;

bool APP_Motor_CreateTask(void);
//只提交请求，PWM操作由电机任务执行
void APP_Motor_SetControl(app_motor_mode_t mode, int8_t direction, uint16_t rpm);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_TASK_H */
