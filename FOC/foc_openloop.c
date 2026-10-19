/*
 * 开环FOC控制的实现文件。
 * 提供了velocityopenloop()函数，用于根据目标机械速度计算电压并更新相位电压。
 * ********************************************************************
 * targetMechanicalSpeed 定义在 freertos_hello.c 的任务函数中。
 * 内部设置 60RPM，等于1转/秒，即2Π rad/s.也就是目标机械速度=6.283rad/s
 * pdMS_TO_TICKS设置为5，参数设定在freertos_hello.c的任务函数种
 * 表示电机任务周期为5ms.
 * pp 在FOC.c中设置为7，表示电机的极对数为7.
 * voltage_limit 在FOC.c中设置为5.0f，表示电压限制5V.
 * ********************************************************************
 * 5ms电机任务周期太长，当前电角度6.283x7=43.98rad/s，每5ms电角度增量约为43.98x0.005=0.22rad=12.6°
 * 也就是三相占空比每次跳约12.6°，一圈电角度只有约29次更新，可能会产生电磁噪声，额外铜耗和发热。提高速度后容易失步
 * 缩短任务周期，尝试将FreeRTOS Tick：200Hz → 1000Hz，电机任务周期缩短为1ms.
 */
#include "FOC/FOC.h"

#include "FreeRTOS.h"
#include "task.h"

float shaft_angle = 0.0f;

/*
 * 保存上一次调用velocityopenloop()时的FreeRTOS节拍。
 */
TickType_t openloop_timestamp = 0U;

float velocityopenloop(float target){
    float Uq;
    float Ts;
    TickType_t now_ts;
    TickType_t elapsedTicks;

    now_ts = xTaskGetTickCount();

    if (openloop_timestamp == 0U){
        Ts = 1.0f / (float)configTICK_RATE_HZ;
    }
    else{
        elapsedTicks = now_ts - openloop_timestamp;
        Ts = (float)elapsedTicks / (float)configTICK_RATE_HZ;
    }

    /*
    * 正常周期是5ms。
    * 只有时间为0或者任务超过50ms没有运行时，才使用默认周期。
    */
    if ((Ts <= 0.0f) || (Ts > 0.050f)){
        Ts = 1.0f / (float)configTICK_RATE_HZ;
    }
    // target是机械角速度，单位rad/s。机械角速度乘以极对数pp，得到电角速度。
    shaft_angle = normalizeAngle(shaft_angle + (float)pp * target * Ts);
    //让Uq等于电压限制。当前voltage_limit最大设置为5V。
    Uq = voltage_limit;
    SetPhaseVoltage(Uq, 0.0f, shaft_angle);
    openloop_timestamp = now_ts;
    return Uq;
}