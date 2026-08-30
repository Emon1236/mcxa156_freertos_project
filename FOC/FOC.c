#include "FOC/FOC.h"

#include "bsp/bsp_motor_pwm.h"

#include <math.h>

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
float voltage_limit = 1.0f;
//在FOC_Init函数中初始化电源电压
float voltage_power_supply = 0.0f;
//电机极对数
int pp = 7;

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
    (void)Ud;//后续闭环控制继续扩展
    //限制Uq在电机允许的最大电压范围内
    Uq = constrain(Uq, -voltage_limit, voltage_limit);
    angle_el = normalizeAngle(angle_el);
    //逆帕克变换
    Ualpha = -Uq * sinf(angle_el);
    Ubeta = Uq * cosf(angle_el);
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

