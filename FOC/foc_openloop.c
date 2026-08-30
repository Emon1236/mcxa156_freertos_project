#include "FOC/FOC.h"

#include "FreeRTOS.h"
#include "task.h"

float shaft_angle = 0.0f;

/*
 * 保存上一次调用velocityopenloop()时的FreeRTOS节拍。
 */
TickType_t openloop_timestamp = 0U;

float velocityopenloop(float target)
{
    float Uq;
    float Ts;
    TickType_t now_ts;
    TickType_t elapsedTicks;

    now_ts = xTaskGetTickCount();

    if (openloop_timestamp == 0U)
    {
        Ts = 1.0f / (float)configTICK_RATE_HZ;
    }
    else
    {
        elapsedTicks = now_ts - openloop_timestamp;

        Ts = (float)elapsedTicks /
            (float)configTICK_RATE_HZ;
    }

    /*
    * 正常周期是5ms。
    * 只有时间为0或者任务超过50ms没有运行时，才使用默认周期。
    */
    if ((Ts <= 0.0f) || (Ts > 0.050f))
    {
        Ts = 1.0f / (float)configTICK_RATE_HZ;
    }

    /*
     * target是机械角速度，单位rad/s。
     *
     * 机械角速度乘以极对数pp，得到电角速度。
     */
    shaft_angle = normalizeAngle(
        shaft_angle +
        (float)pp * target * Ts);

    /*
     * 商家开环例程直接让Uq等于电压限制。
     *
     * 当前voltage_limit最大设置为5V。
     */
    Uq = voltage_limit;

    SetPhaseVoltage(
        Uq,
        0.0f,
        shaft_angle);

    openloop_timestamp = now_ts;

    return Uq;
}