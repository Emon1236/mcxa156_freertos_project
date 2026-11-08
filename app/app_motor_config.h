#ifndef APP_MOTOR_CONFIG_H
#define APP_MOTOR_CONFIG_H

#define APP_MOTOR_BUS_VOLTAGE      12.6f
#define APP_MOTOR_MAX_VOLTAGE      5.0f
#define APP_MOTOR_POLE_PAIRS       7U
#define APP_MOTOR_SENSOR_DIRECTION 1
#define APP_MOTOR_PWM_HZ           15000U
#define APP_MOTOR_ALIGN_VOLTAGE    1.0f

//新增开环参数
#define APP_MOTOR_OPENLOOP_RPM     60.0f//目标机械转速
#define APP_MOTOR_OPENLOOP_UQ      1.0f//驱动电压

//沿用当前工程中的位置PD参数
#define APP_POSITION_KP           4.5f
#define APP_POSITION_KD           0.02f
#define APP_POSITION_UQ_LIMIT     1.0f
#define APP_POSITION_RAMP         0.0f
#endif
