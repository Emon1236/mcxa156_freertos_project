#include "low_filter.h"
#include "task.h"

LowPassFilter::LowPassFilter(float time_constant)
    :Tf(time_constant)
    ,timestamp_prev(0)
    ,y_prev(0.0f)
    ,initialized(false)
{
        //构造时只初始化数据，首次调用时再获取时间
}

float LowPassFilter::operator()(float x){
    const TickType_t timestamp = xTaskGetTickCount();
    // 首次调用直接用输入初始化，避免从 0 开始爬升。
    if (!initialized){
        initialized = true;
        timestamp_prev = timestamp;
        y_prev = x;
        return x;
    }
    // 非正时间常数：直接输出输入，关闭滤波。
    if (Tf <= 0.0f){
        timestamp_prev = timestamp;
        y_prev = x;
        return x;
    }
    // 使用无符号差值，允许正常跨越 tick 计数回绕。
    const TickType_t elapsed_ticks = (TickType_t)(timestamp - timestamp_prev);

    // 同一个 tick 内重复调用，没有可测得的新时间间隔。
    if (elapsed_ticks == 0){
        return y_prev;
    }
    const float dt = (float)elapsed_ticks / (float)configTICK_RATE_HZ;

    timestamp_prev = timestamp;
    // 超过 300 ms 未更新则重新同步输入。
    if (dt > 0.3f){
        y_prev = x;
        return x;
    }
    //根据运行时间和设定时间计算时间系数α
    const float alpha = Tf / (Tf + dt);
    //低通滤波公式
    const float y = alpha * y_prev + (1.0f - alpha) * x;

    y_prev = y;
    return y;
}
void LowPassFilter::Reset(void){
    timestamp_prev = 0;
    y_prev = 0.0f;
    initialized = false;    
}

float LowPassFilter::Update(float x,float dt){
    if(dt <= 0.0f){
        return y_prev;
    }
    if(!initialized || Tf <= 0.0f || dt > 0.3f){
        initialized = true;
        y_prev = x;
        return x;        
    }
    const float alpha = Tf / (Tf + dt);

    y_prev = alpha * y_prev + (1.0f - alpha) * x;
    return y_prev;
}
