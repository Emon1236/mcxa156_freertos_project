#ifndef PID_H
#define PID_H

#include "FreeRTOS.h"

// Adapted from DengFOC_Lib, V0.2, pid.h.
class PIDController{

    public:
        PIDController(float P, float I, float D, float ramp, float limit);
        ~PIDController() = default;

        float operator() (float error);
        float UpdatePD(float error, float velocity, float dt);
        void Reset(void);

        float P; //!< 比例增益
        float I; //!< 积分增益
        float D; //!< 微分增益
        float output_ramp; //!< 输出变化率限制，单位为输出单位/s
        float limit; //!< 输出限幅

    protected:
        float error_prev; //!< 上次误差
        float output_prev; //!< 上次输出
        float integral_prev; //!< 上次积分分量
        TickType_t timestamp_prev; //!< 上次执行的FreeRTOS tick
};

#endif
