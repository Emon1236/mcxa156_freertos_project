#ifndef APP_POSITION_TASK_H
#define APP_POSITION_TASK_H

#include "FOC/FOC.h"

//正常固件校准后自动旋转；调参构建保持手动启动。
#ifndef APP_POSITION_AUTO_RUN
#define APP_POSITION_AUTO_RUN    1U
#endif
#define APP_POSITION_SPEED_RPS   2.0f  //机械转速，圈/秒；负值反转

//P、D可通过接口在运行时修改。
#define APP_POSITION_KP          4.5f
#define APP_POSITION_KD          0.02f
#define APP_POSITION_UQ_LIMIT    1.0f
#define APP_POSITION_RAMP        0.0f

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    APP_POSITION_WAIT_SENSOR = 0U,
    APP_POSITION_ALIGNING,
    APP_POSITION_READY,
    APP_POSITION_RUNNING,
    APP_POSITION_FAULT
} app_position_state_t;

typedef foc_position_params_t app_position_params_t;

bool APP_Position_CreateTask(void);
bool APP_Position_SetParams(const app_position_params_t *params);
app_position_params_t APP_Position_GetParams(void);

/* 校准完成后显式启动，目标取最新机械位置。 */
bool APP_Position_StartHold(void);
/* 从当前位置开始匀速推进目标，单位为机械rad/s；0表示位置保持。 */
bool APP_Position_StartRotate(float speedRad);
/* 单圈机械目标，单位rad，正方向与FOC一致；取消匀速推进，不自动启动。 */
bool APP_Position_SetTarget(float targetRad);
void APP_Position_Stop(void);
app_position_state_t APP_Position_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
