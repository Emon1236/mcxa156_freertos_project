#include "FreeRTOS.h"

#ifndef LOWPASS_FILTER_H
#define LOWPASS_FILTER_H

//低通滤波器类型

class LowPassFilter{
    public:
        LowPassFilter(float time_constant);
        ~LowPassFilter() = default;

        float operator() (float x);
        //dt单位为秒
        float Update(float x, float dt);
        void Reset(void);

        float Tf;//低通滤波时间常数

    protected:
        TickType_t timestamp_prev;//最后执行的时间戳
        float y_prev;//上一个循环中过滤后的值
        bool initialized;
};

#endif