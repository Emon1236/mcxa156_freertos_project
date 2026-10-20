#ifndef FOC_H
#define FOC_H

#include "fsl_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern float Ua;
extern float Ub;
extern float Uc;

extern float Ualpha;
extern float Ubeta;

extern float dc_a;
extern float dc_b;
extern float dc_c;
//Uq最大允许电压，限制10V
extern float voltage_limit;
//实际直流母线电压，12.6V
extern float voltage_power_supply;
//电机极对数
extern int pp;

extern float shaft_angle;

float constrain(float amt, float low, float high);
float normalizeAngle(float angle);
void SetPhaseVoltage(float Uq, float Ud, float angle_el);
status_t FOC_Init(float power);

//开环速度控制
float velocityopenloop(float target);
// 自动检测传感器方向并校准电角度零点
status_t FOC_AlignSensor(int *dir, float *zero_angle);
// 根据 AS5600 原始计数计算当前电角度
status_t FOC_CalcElectricalAngle(uint16_t raw_angle, float *angle_el);

#ifdef __cplusplus
}
#endif

#endif // FOC_H