#include "FOC.h"

#include <math.h>
#include <stddef.h>

//将传入的参数设定在指定范围内
static float foc_limit(float value, float low, float high){
    return value < low ? low : (value > high ? high : value);
}
//将角度归一化到0~2Π
float FOC_NormalizeAngle(float angle){
    float result = fmodf(angle, FOC_TWO_PI);
    return result >= 0.0f ? result : result + FOC_TWO_PI;
}
//将角度差转换到(-pi,pi]用于寻找最短转动方向
float FOC_WrapAngle(float angle){
    float result = FOC_NormalizeAngle(angle);
    return result > FOC_PI ? result - FOC_TWO_PI : result;
}
//初始化电机上下文，设置母线电压，最终电压上限、极对数和传感器方向；清空历史和校准状态
bool FOC_Init(foc_motor_t *motor, float busVoltage, float maxVoltage, uint8_t polePairs, int8_t direction){
    if (motor == NULL || 
        !isfinite(busVoltage) || 
        !isfinite(maxVoltage) ||
        busVoltage <= 0.0f || 
        maxVoltage <= 0.0f ||
        polePairs == 0U || 
        (direction != 1 && direction != -1)){
        return false;
    }

    *motor = (foc_motor_t){0};
    motor->busVoltage = busVoltage;
    motor->maxVoltage = maxVoltage;
    motor->polePairs = polePairs;
    motor->direction = direction;
    motor->position.voltageLimit = maxVoltage;
    return true;
}
//清空位置控制历史和开环电角度，保留参数与校准结果
//为后续的UI中停止，切换控制模式的功能使用
void FOC_ResetControl(foc_motor_t *motor){
    motor->positionState = (foc_position_state_t){0};
    motor->openAngle = 0.0f;
}
//给传感器角度应用方向，得到统一方向的机械角度
float FOC_MechanicalAngle(const foc_motor_t *motor, float sensorAngle){
    return FOC_NormalizeAngle((float)motor->direction * sensorAngle);
}
//根据传感器机械角度、方向、极对数和零偏，计算电角度
bool FOC_SetZeroAngle(foc_motor_t *motor, float sensorAngle){
    if (!isfinite(sensorAngle)){
        return false;
    }
    motor->zeroAngle = FOC_NormalizeAngle((float)motor->polePairs * (float)motor->direction * sensorAngle);
    motor->calibrated = true;
    return true;
}
//根据传感器机械角度、方向、极对数和零偏，计算电角度
bool FOC_CalcElectricalAngle(const foc_motor_t *motor, float sensorAngle, float *electricalAngle){
    if (!motor->calibrated || electricalAngle == NULL || !isfinite(sensorAngle)){
        return false;
    }
    *electricalAngle = FOC_NormalizeAngle((float)motor->polePairs * (float)motor->direction * sensorAngle - motor->zeroAngle);
    return true;
}
//限制dp电压矢量，完成逆park变换，逆clarke变换，计算三相占空比
bool FOC_Modulate(const foc_motor_t *motor, float uq, float ud, float electricalAngle, foc_pwm_t *pwm){
    if (pwm == NULL || 
        motor->busVoltage <= 0.0f || 
        motor->maxVoltage <= 0.0f ||
        !isfinite(uq) || 
        !isfinite(ud) || 
        !isfinite(electricalAngle)){
        return false;
    }

    //限制整个dq电压矢量；校准时也可以直接使用此接口。
    float magnitude = sqrtf(uq * uq + ud * ud);
    if (magnitude > motor->maxVoltage){
        float scale = motor->maxVoltage / magnitude;
        uq *= scale;
        ud *= scale;
    }

    float angle = FOC_NormalizeAngle(electricalAngle);
    float sine = sinf(angle);
    float cosine = cosf(angle);
    float Ua = ud * cosine - uq * sine;
    float Ub = ud * sine + uq * cosine;
    float center = motor->busVoltage * 0.5f;

    pwm->a = foc_limit((Ua + center) / motor->busVoltage, 0.0f, 1.0f);
    pwm->b = foc_limit(((-Ua + 1.73205080757f * Ub) * 0.5f + center) /
                       motor->busVoltage, 0.0f, 1.0f);
    pwm->c = foc_limit(((-Ua - 1.73205080757f * Ub) * 0.5f + center) /
                       motor->busVoltage, 0.0f, 1.0f);
    return true;
}
//根据目标机械速度和dt推进开环电角度，再计算PWM
bool FOC_OpenLoopUpdate(foc_motor_t *motor, float speed, float uq, float dt, foc_pwm_t *pwm){
    if (!isfinite(speed) || !isfinite(uq) || !isfinite(dt) || dt <= 0.0f){
        return false;
    }
    float angle = FOC_NormalizeAngle(motor->openAngle + (float)motor->polePairs * speed * dt);
    if (!FOC_Modulate(motor, uq, 0.0f, angle, pwm)){
        return false;
    }
    motor->openAngle = angle;
    return true;
}
//检查P,D,工作限幅和输出斜率参数是否合法
bool FOC_PositionParamsValid(const foc_position_params_t *params, float maxVoltage){
    return params != NULL && isfinite(params->kp) && isfinite(params->kd) &&
           isfinite(params->voltageLimit) && isfinite(params->outputRamp) &&
           params->kp >= 0.0f && params->kd >= 0.0f &&
           params->voltageLimit > 0.0f && params->voltageLimit <= maxVoltage &&
           params->outputRamp >= 0.0f;
}
//验证并保存位置环参数，不清空控制历史
bool FOC_SetPositionParams(foc_motor_t *motor, const foc_position_params_t *params){
    if (!FOC_PositionParamsValid(params, motor->maxVoltage)){
        return false;
    }
    motor->position = *params;
    return true;
}
//用当前反馈和目标位置简历控制基准，清楚上次输出
bool FOC_PositionReset(foc_motor_t *motor, const foc_feedback_t *feedback, float target){
    if (!isfinite(feedback->angle) || !isfinite(target)){
        return false;
    }
    motor->positionState.lastAngle = FOC_MechanicalAngle(motor, feedback->angle);
    motor->positionState.error = FOC_WrapAngle(target - motor->positionState.lastAngle);
    motor->positionState.lastOutput = 0.0f;
    return true;
}
//计算位置误差、PD输出、电角度和PWM；支持固定位置及连续旋转目标
bool FOC_PositionUpdate(foc_motor_t *motor, const foc_feedback_t *feedback, float target, float targetSpeed, float dt, foc_pwm_t *pwm){
    float electricalAngle;
    if (!isfinite(dt) || 
        dt <= 0.0f || 
        dt > 0.1f ||
        !isfinite(target) || 
        !isfinite(targetSpeed) || 
        !isfinite(feedback->velocity) ||
        !FOC_CalcElectricalAngle(motor, feedback->angle, &electricalAngle)){
        return false;
    }

    foc_position_state_t next = motor->positionState;
    foc_position_state_t *state = &next;
    const foc_position_params_t *params = &motor->position;
    float angle = FOC_MechanicalAngle(motor, feedback->angle);
    float velocity = (float)motor->direction * feedback->velocity;

    if (targetSpeed != 0.0f){
        //累计位移差，跨圈或落后超过半圈时仍保持目标方向。
        state->error += targetSpeed * dt - FOC_WrapAngle(angle - state->lastAngle);
    }
    else{
        state->error = FOC_WrapAngle(target - angle);
    }

    //D作用于速度偏差，固定位置时targetSpeed为0。
    float output = params->kp * state->error + params->kd * (targetSpeed - velocity);
    output = foc_limit(output, -params->voltageLimit, params->voltageLimit);
    if (params->outputRamp > 0.0f){
        float change = params->outputRamp * dt;
        output = foc_limit(output, state->lastOutput - change, state->lastOutput + change);
    }
    //运行中调低限幅时，立即服从新的限制。
    output = foc_limit(output, -params->voltageLimit, params->voltageLimit);
    if (!FOC_Modulate(motor, output, 0.0f, electricalAngle, pwm)){
        return false;
    }
    state->lastAngle = angle;
    state->lastOutput = output;
    motor->positionState = next;
    return true;
}
