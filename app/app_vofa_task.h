#ifndef APP_VOFA_TASK_H
#define APP_VOFA_TASK_H

#include <stdbool.h>

#define APP_VOFA_BAUDRATE  115200U
#define APP_VOFA_PERIOD_MS 10U  //100Hz发送，与电机控制周期独立。

#ifdef __cplusplus
extern "C" {
#endif

bool APP_VOFA_CreateTask(void);

#ifdef __cplusplus
}
#endif

#endif
