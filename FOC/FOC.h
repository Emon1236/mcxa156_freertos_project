#ifndef FOC_H
#define FOC_H

#include <stdbool.h>
#include <stdint.h>

#define FOC_PI     3.14159265359f
#define FOC_TWO_PI (2.0f * FOC_PI)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    float kp;            //V/rad
    float kd;            //V/(rad/s)
    float voltageLimit;  //位置环Uq限幅，V
    float outputRamp;    //输出变化率，V/s，0表示不限制
} foc_position_params_t;

typedef struct{
    float lastAngle;
    float error;
    float lastOutput;
} foc_position_state_t;

typedef struct{
    float busVoltage;
    float maxVoltage;
    uint8_t polePairs;
    int8_t direction;

    float zeroAngle;
    bool calibrated;
    float openAngle;  //开环电角度

    foc_position_params_t position;
    foc_position_state_t positionState;
} foc_motor_t;

typedef struct{
    float angle;     //传感器机械角度，rad，尚未应用direction
    float velocity;  //同一传感器方向的机械速度，rad/s
} foc_feedback_t;

typedef struct{
    float a;
    float b;
    float c;
} foc_pwm_t;

float FOC_NormalizeAngle(float angle);
float FOC_WrapAngle(float angle);

/* 仅初始化算法，不操作PWM、编码器或操作系统。 */
bool FOC_Init(foc_motor_t *motor, float busVoltage, float maxVoltage,
              uint8_t polePairs, int8_t direction);
void FOC_ResetControl(foc_motor_t *motor);
float FOC_MechanicalAngle(const foc_motor_t *motor, float sensorAngle);
bool FOC_SetZeroAngle(foc_motor_t *motor, float sensorAngle);
bool FOC_CalcElectricalAngle(const foc_motor_t *motor, float sensorAngle,
                             float *electricalAngle);
bool FOC_Modulate(const foc_motor_t *motor, float uq, float ud,
                  float electricalAngle, foc_pwm_t *pwm);

/* speed为机械rad/s，dt为实际秒数，uq独立于最终电压上限。 */
bool FOC_OpenLoopUpdate(foc_motor_t *motor, float speed, float uq,
                        float dt, foc_pwm_t *pwm);

bool FOC_PositionParamsValid(const foc_position_params_t *params, float maxVoltage);
bool FOC_SetPositionParams(foc_motor_t *motor, const foc_position_params_t *params);
/* target为统一方向后的机械角度；每次重新启动或切换到位置控制时调用。 */
bool FOC_PositionReset(foc_motor_t *motor, const foc_feedback_t *feedback, float target);
/* targetSpeed为0时走单圈最短路径，非0时连续推进位置目标。 */
bool FOC_PositionUpdate(foc_motor_t *motor, const foc_feedback_t *feedback,
                        float target, float targetSpeed, float dt, foc_pwm_t *pwm);

/* 上下文由调用方独占；并发命令和反馈新鲜度由App处理。 */
#ifdef __cplusplus
}
#endif

#endif // FOC_H
