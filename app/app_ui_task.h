#ifndef APP_UI_TASK_H
#define APP_UI_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 创建UI任务。
 * 返回true表示任务创建成功。
 */
bool APP_UI_CreateTask(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_TASK_H */