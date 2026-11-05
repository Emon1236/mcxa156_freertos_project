#include "pid.h"
#include "task.h"

// Adapted from DengFOC_Lib, V0.2, pid.cpp.
// https://github.com/ToanTech/DengFOC_Lib
#define _constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

PIDController::PIDController(float P, float I, float D, float ramp, float limit)
    : P(P)
    , I(I)
    , D(D)
    , output_ramp(ramp)
    , limit(limit)
    , error_prev(0.0f)
    , output_prev(0.0f)
    , integral_prev(0.0f)
{
    // 构造函数只初始化数据，不依赖调度器或外设。
    timestamp_prev = 0;
}

float PIDController::operator() (float error){
    // 使用FreeRTOS tick替代Arduino micros()，Ts单位为秒。
    TickType_t timestamp_now = xTaskGetTickCount();
    TickType_t elapsed_ticks = (TickType_t)(timestamp_now - timestamp_prev);
    float Ts = (float)elapsed_ticks / (float)configTICK_RATE_HZ;
    if(Ts <= 0 || Ts > 0.5f) Ts = 1.0f / (float)configTICK_RATE_HZ;

    // P环
    float proportional = P * error;
    // Tustin梯形积分（I环）
    float integral = integral_prev + I*Ts*0.5f*(error + error_prev);
    integral = _constrain(integral, -limit, limit);
    // D环
    float derivative = D*(error - error_prev)/Ts;
    // 合成输出并限幅
    float output = proportional + integral + derivative;
    output = _constrain(output, -limit, limit);
    if(output_ramp > 0){
        // 限制输出变化率；输出为Uq时，单位是V/s。
        float output_rate = (output - output_prev)/Ts;
        if (output_rate > output_ramp)
            output = output_prev + output_ramp*Ts;
        else if (output_rate < -output_ramp)
            output = output_prev - output_ramp*Ts;
    }
    // 保存下一次计算需要的状态
    integral_prev = integral;
    output_prev = output;
    error_prev = error;
    timestamp_prev = timestamp_now;
    return output;
}

void PIDController::Reset(void){
    error_prev = 0.0f;
    output_prev = 0.0f;
    integral_prev = 0.0f;
    timestamp_prev = 0;
}

float PIDController::UpdatePD(float error, float velocity, float dt){
    if (dt <= 0.0f || dt > 0.1f){
        Reset();
        return 0.0f;
    }

    //速度单位rad/s；D作用于测量，目标位置改变时不产生微分尖峰。
    float output = P * error - D * velocity;
    output = _constrain(output, -limit, limit);

    if (output_ramp > 0.0f){
        float change = output_ramp * dt;
        output = _constrain(output, output_prev - change, output_prev + change);
    }
    //运行中调小限幅时，仍然立即满足新的电压限制。
    output = _constrain(output, -limit, limit);

    error_prev = error;
    output_prev = output;
    integral_prev = 0.0f;
    return output;
}
