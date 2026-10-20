#include "FOC/FOC.h"
#include "AS5600.h"   

#include "bsp/bsp_motor_pwm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>
#include <stdint.h>
#include <stddef.h>
//定义圆周率
#define PI 3.14159265359f

float Ua = 0.0f;
float Ub = 0.0f;
float Uc = 0.0f;

float Ualpha = 0.0f;
float Ubeta = 0.0f;

float dc_a = 0.0f;
float dc_b = 0.0f;
float dc_c = 0.0f;

//电机允许的最大Uq电压，施加给旋转电压矢量的最大赋值，直观感受就是控制电机扭矩的物理量
float voltage_limit = 2.0f;
//在FOC_Init函数中初始化电源电压
float voltage_power_supply = 0.0f;
//电机极对数,机械角度=pp*电角度
int pp = 7;

// 方向：+1 或 -1；0 表示尚未完成校准
static int sensor_dir = 0;
// 电角度零点偏差，单位 rad
static float zero_electric_angle = 0.0f;
// 校准时施加的 Uq，必须不超过 voltage_limit
#define ALIGN_VOLTAGE  1.0f

// 270°
#define ALIGN_ANGLE    (3.0f * PI / 2.0f)


float constrain(float amt, float low, float high){
    return ((amt < low) ? low : ((amt > high) ? high : amt));
}

float normalizeAngle(float angle){
    float normalizedAngle;
    normalizedAngle = fmodf(angle, 2.0f * PI);
    return ((normalizedAngle >= 0.0f) ? normalizedAngle : (normalizedAngle + 2.0f * PI));
}

void SetPwm(float phaseUa, float phaseUb, float phaseUc){
    //Ua、Ub、Uc是相对于功率板地的电压，因此范围必须是0~母线电压。
    //这里不能限制到0~10，旋转电压矢量不是加入母线重点后的相对地电压
    Ua = constrain(phaseUa, 0.0f, voltage_power_supply);
    Ub = constrain(phaseUb, 0.0f, voltage_power_supply);
    Uc = constrain(phaseUc, 0.0f, voltage_power_supply);

    dc_a = constrain(Ua / voltage_power_supply, 0.0f, 1.0f);
    dc_b = constrain(Ub / voltage_power_supply, 0.0f, 1.0f);
    dc_c = constrain(Uc / voltage_power_supply, 0.0f, 1.0f);

    (void)BSP_MotorPwm_SetDuty(dc_a, dc_b, dc_c);
}

void SetPhaseVoltage(float Uq, float Ud, float angle_el){
    //(void)Ud;//后续闭环控制继续扩展
    //限制Uq在电机允许的最大电压范围内
    // Uq = constrain(Uq, -voltage_limit, voltage_limit);
    // Ud = constrain(Ud, -voltage_limit, voltage_limit);
    /*
    magnitude是d/q电压指令的矢量幅值，限制整个电压矢量，而不是分别将ud和uq限制到±1V后就认为安全，当两者同时非零时，合成幅值可能超限。
    */
    float magnitude = sqrtf(Ud * Ud + Uq * Uq);

    if (magnitude > voltage_limit && magnitude > 0.0f) {
        float scale = voltage_limit / magnitude;
        Ud *= scale;
        Uq *= scale;
    }
    angle_el = normalizeAngle(angle_el);

    //逆帕克变换
    Ualpha = Ud * cosf(angle_el) - Uq * sinf(angle_el);
    Ubeta = Ud * sinf(angle_el) + Uq * cosf(angle_el);
    //逆克拉克变换
    //voltage_power_supply / 2用于把正负相电压，平移到PWM可以表示的0~母线电压范围。
     Ua = Ualpha + voltage_power_supply / 2.0f;
     Ub = (sqrtf(3.0f) * Ubeta - Ualpha) / 2.0f + voltage_power_supply / 2.0f;
     Uc = (-sqrtf(3.0f) * Ubeta - Ualpha) / 2.0f + voltage_power_supply / 2.0f;
    SetPwm(Ua, Ub, Uc);
}

status_t FOC_Init(float power){
    status_t status;
    if (power <= 0.0f){
        return kStatus_InvalidArgument;
    }
    //这里传入实际母线电压12.6V。
    voltage_power_supply = power;
    //这里配置15kHz PWM
    status = BSP_MotorPwm_Init(15000U);

    if (status != kStatus_Success){
        return status;
    }
    //初始三相为50%，线电压为0
    status = BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f);
    if (status != kStatus_Success){
        return status;
    }

    return BSP_MotorPwm_Start();
}

status_t FOC_AlignSensor(int *dir, float *zero_angle)
{
    uint16_t raw_a, raw_b, raw_zero;
    status_t status;

    // 检查参数与初始化状态
    if (dir == NULL || zero_angle == NULL ||
        voltage_power_supply <= 0.0f ||
        voltage_limit < ALIGN_VOLTAGE) {
        return kStatus_InvalidArgument;
    }

    sensor_dir = 0;  // 标记为尚未校准

    // ① 固定磁场到 270°，读取初始角度
    SetPhaseVoltage(ALIGN_VOLTAGE, 0.0f, ALIGN_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(500));

    status = AS5600_ReadRawAngle(&raw_a);
    if (status != kStatus_Success) goto end;

    // ② 磁场正向旋转 90° 电角度
    SetPhaseVoltage(ALIGN_VOLTAGE, 0.0f,
                    ALIGN_ANGLE + PI / 2.0f);
    vTaskDelay(pdMS_TO_TICKS(500));

    status = AS5600_ReadRawAngle(&raw_b);
    if (status != kStatus_Success) goto end;

    // ③ 计算编码器计数变化，并处理 0/4095 跨越
    int delta = (int)raw_b - (int)raw_a;

    if (delta > 2048)  delta -= 4096;
    if (delta < -2048) delta += 4096;

    // 转子变化太小，不能可靠判断方向
    if (delta > -20 && delta < 20) {
        status = kStatus_Fail;
        goto end;
    }

    int detected_dir = (delta > 0) ? 1 : -1;

    // ④ 回到 270°，重新对齐并读取零点
    SetPhaseVoltage(ALIGN_VOLTAGE, 0.0f, ALIGN_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(500));

    status = AS5600_ReadRawAngle(&raw_zero);
    if (status != kStatus_Success) goto end;

    // 编码器原始值转换为机械弧度
    float mechanical_angle =
        (float)raw_zero * 2.0f * PI / 4096.0f;

    // 计算电角度零点偏差
    zero_electric_angle = normalizeAngle(
        (float)(pp * detected_dir) * mechanical_angle
    );

    sensor_dir = detected_dir;

    // 将结果返回给调用者，便于串口调试
    *dir = sensor_dir;
    *zero_angle = zero_electric_angle;

end:
    // 校准结束，恢复三相 50% 占空比
    SetPhaseVoltage(0.0f, 0.0f, 0.0f);
    return status;
}

status_t FOC_CalcElectricalAngle(uint16_t raw_angle, float *angle_el){
    if (angle_el == NULL || sensor_dir == 0) {
        return kStatus_Fail;
    }
    float mechanical_angle = (float)raw_angle * 2.0f * PI / 4096.0f;
    *angle_el = normalizeAngle((float)(pp * sensor_dir) * mechanical_angle - zero_electric_angle);
    return kStatus_Success;
}
