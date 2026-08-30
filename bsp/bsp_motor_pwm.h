#ifndef BSP_MOTOR_PWM_H
#define BSP_MOTOR_PWM_H
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化三个同步PWM输出：
 *
 * A相：P3_6  / PWM0_A0
 * B相：P3_8  / PWM0_A1
 * C相：P3_10 / PWM0_A2
*/
status_t BSP_MotorPwm_Init(uint32_t pwmFrequencyHz);
//同步更新三相占空比，变量范围在0.0~1.0f
status_t BSP_MotorPwm_SetDuty(float dutyA, float dutyB, float dutyC);
//启动三个PWM子模块。
status_t BSP_MotorPwm_Start(void);
//停止三个PWM子模块。
status_t BSP_MotorPwm_Stop(void);


#ifdef __cplusplus
}
#endif

#endif // BSP_MOTOR_PWM_H