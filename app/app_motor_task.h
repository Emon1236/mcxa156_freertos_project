#ifndef APP_MOTOR_TASK_H
#define APP_MOTOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


bool Foc_openloop_Createdtask(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_TASK_H */