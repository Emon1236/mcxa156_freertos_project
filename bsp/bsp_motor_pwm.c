/**
 * @file bsp_motor_pwm.c
 * 电机PWM的驱动端口使用的FlexPWM0模块，映射端口为P3_6、P3_8、P3_10。
 * 需要注意的是，当前硬件的电机驱动板要给motor_en接入3.3V使能才可以驱动电机
 * 至于原本原理图种显示的M0_EN和M1_EN(现motor_en)则忽略
 *************************************************************
 * 
 */


#include "bsp/bsp_motor_pwm.h"

#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_pwm.h"
#include "fsl_reset.h"

#include <stdbool.h>
#include <stdint.h>
// PWM的基地址为FlexPWM0
#define BSP_MOTOR_PWM_BASE FLEXPWM0

static bool s_motorPwmInitialized = false;
static bool s_motorPwmRunning = false;

status_t BSP_MotorPwm_Init(uint32_t pwmFrequencyHz){
    port_pin_config_t pwmPinConfig;
    pwm_config_t pwmConfig;
    pwm_signal_param_t pwmSignal;
    uint32_t pwmSourceClockHz;
    status_t status;
    if (s_motorPwmInitialized){
        return kStatus_Success;
    }
    if (pwmFrequencyHz == 0U){
        return kStatus_InvalidArgument;
    }
    /*
     * 配置P3_6、P3_8、P3_10为FlexPWM0输出。
     * 三个引脚的PWM复用功能都是ALT5。
     */
    CLOCK_EnableClock(kCLOCK_GatePORT3);
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kFLEXPWM0_RST_SHIFT_RSTn);  

    pwmPinConfig.pullSelect = kPORT_PullDisable;
    pwmPinConfig.pullValueSelect = kPORT_LowPullResistor;
    pwmPinConfig.slewRate = kPORT_FastSlewRate;
    pwmPinConfig.passiveFilterEnable = kPORT_PassiveFilterDisable;
    pwmPinConfig.openDrainEnable = kPORT_OpenDrainDisable;
    pwmPinConfig.driveStrength = kPORT_LowDriveStrength;
    pwmPinConfig.driveStrength1 = kPORT_NormalDriveStrength;
    pwmPinConfig.mux = kPORT_MuxAlt5;
    pwmPinConfig.inputBuffer = kPORT_InputBufferEnable;
    pwmPinConfig.invertInput = kPORT_InputNormal;
    pwmPinConfig.lockRegister = kPORT_UnlockRegister;
    //A相：P3_6 -> PWM0_A0
    PORT_SetPinConfig(PORT3, 6U, &pwmPinConfig);    
    //B相：P3_8 -> PWM0_A1
    PORT_SetPinConfig(PORT3, 8U, &pwmPinConfig);
    //C相：P3_10 -> PWM0_A2
    PORT_SetPinConfig(PORT3, 10U, &pwmPinConfig);

    pwmSourceClockHz = CLOCK_GetFreq(kCLOCK_MainClk);
    if (pwmSourceClockHz == 0U){
        return kStatus_Fail;
    }
    PWM_GetDefaultConfig(&pwmConfig);
    //当前转接板按三路单PWM输入处理，因此每个子模块只使用PWM A输出。
    pwmConfig.pairOperation = kPWM_Independent;
    //中心对其PWM
    pwmConfig.reloadLogic = kPWM_ReloadPwmFullCycle;
    //调试器暂停CPU时停止PWM
    pwmConfig.enableDebugMode = false;
    pwmConfig.prescale = kPWM_Prescale_Divide_1;
    //子模块0使用总线时钟，作为另外两个子模块的同步源
    pwmConfig.clockSource = kPWM_BusClock;
    pwmConfig.initializationControl = kPWM_Initialize_LocalSync;
    status = PWM_Init(BSP_MOTOR_PWM_BASE, kPWM_Module_0, &pwmConfig);
    if (status != kStatus_Success){
        return status;
    }
    //子模块1，2使用子模块0的时钟和同步信号
    pwmConfig.clockSource = kPWM_Submodule0Clock;
    pwmConfig.initializationControl = kPWM_Initialize_MasterSync;
    status = PWM_Init(BSP_MOTOR_PWM_BASE,kPWM_Module_1,&pwmConfig);
    if (status != kStatus_Success){
        return status;
    }
    status = PWM_Init(BSP_MOTOR_PWM_BASE,kPWM_Module_2,&pwmConfig);
    if (status != kStatus_Success){
        return status;
    }
    /*
    * MCXA156 FlexPWM 的 DISMAP 寄存器复位后，故障输入默认会映射到PWM输出。
    * 当前开环测试没有使用硬件故障输入，因此先取消三个PWM A通道的
    * 故障映射，避免未配置的FAULT输入把输出一直压成低电平。
    *
    * 后续增加过流保护时，应重新配置真实的FAULT输入，
    * 而不是继续永久关闭故障保护。
    */
    PWM_SetupFaultDisableMap(BSP_MOTOR_PWM_BASE,
                         kPWM_Module_0,
                         kPWM_PwmA,
                         kPWM_faultchannel_0,
                         0U);
    PWM_SetupFaultDisableMap(BSP_MOTOR_PWM_BASE,
                         kPWM_Module_1,
                         kPWM_PwmA,
                         kPWM_faultchannel_0,
                         0U);
    PWM_SetupFaultDisableMap(BSP_MOTOR_PWM_BASE,
                         kPWM_Module_2,
                         kPWM_PwmA,
                         kPWM_faultchannel_0,
                         0U);
    //三相都从50%占空比开始，当三相占空比相同时，三相之间的线电压为0，不会形成旋转电压矢量
    pwmSignal.pwmChannel = kPWM_PwmA;
    pwmSignal.level = kPWM_HighTrue;
    pwmSignal.dutyCyclePercent = 50U;
    pwmSignal.deadtimeValue = 0U;
    pwmSignal.faultState = kPWM_PwmFaultState0;
    pwmSignal.pwmchannelenable = true;   
    
    status = PWM_SetupPwm(BSP_MOTOR_PWM_BASE,kPWM_Module_0,&pwmSignal,1U,
        kPWM_SignedCenterAligned,pwmFrequencyHz,pwmSourceClockHz);
    if (status != kStatus_Success){
        return status;
    }
    status = PWM_SetupPwm(BSP_MOTOR_PWM_BASE,kPWM_Module_1,&pwmSignal,1U,
        kPWM_SignedCenterAligned,pwmFrequencyHz,pwmSourceClockHz);
    if (status != kStatus_Success){
        return status;
    }

    status = PWM_SetupPwm(BSP_MOTOR_PWM_BASE,kPWM_Module_2,&pwmSignal,1U,
        kPWM_SignedCenterAligned,pwmFrequencyHz,pwmSourceClockHz);
    if (status != kStatus_Success){
        return status;
    }

    PWM_SetPwmLdok(BSP_MOTOR_PWM_BASE,
        kPWM_Control_Module_0 |
        kPWM_Control_Module_1 |
        kPWM_Control_Module_2,
        true);
    s_motorPwmInitialized = true;
    return kStatus_Success;
}

status_t BSP_MotorPwm_SetDuty(float dutyA, float dutyB, float dutyC){
    uint16_t compareA;
    uint16_t compareB;
    uint16_t compareC;
    if (!s_motorPwmInitialized){
        return kStatus_Fail;
    }
    //占空比限制
    if (dutyA < 0.0f) dutyA = 0.0f;
    else if (dutyA > 1.0f) dutyA = 1.0f;
    if (dutyB < 0.0f) dutyB = 0.0f;
    else if (dutyB > 1.0f) dutyB = 1.0f;
    if (dutyC < 0.0f) dutyC = 0.0f;
    else if (dutyC > 1.0f) dutyC = 1.0f;
    //MCU SDK接口使用0~65535表示0~100%
    compareA = (uint16_t)(dutyA * 65535.0f);
    compareB = (uint16_t)(dutyB * 65535.0f);
    compareC = (uint16_t)(dutyC * 65535.0f);
    
    //清除三个字模块的加载标志，修改缓冲寄存器
    PWM_SetPwmLdok(BSP_MOTOR_PWM_BASE,
        kPWM_Control_Module_0 |
        kPWM_Control_Module_1 |
        kPWM_Control_Module_2,
        false);
    PWM_UpdatePwmDutycycleHighAccuracy(BSP_MOTOR_PWM_BASE,kPWM_Module_0,kPWM_PwmA,
        kPWM_SignedCenterAligned,compareA); 
    PWM_UpdatePwmDutycycleHighAccuracy(BSP_MOTOR_PWM_BASE,kPWM_Module_1,kPWM_PwmA,
        kPWM_SignedCenterAligned,compareB); 
    PWM_UpdatePwmDutycycleHighAccuracy(BSP_MOTOR_PWM_BASE,kPWM_Module_2,kPWM_PwmA,
        kPWM_SignedCenterAligned,compareC); 

    //重新设置三个子模块的加载标志，使修改生效
    PWM_SetPwmLdok(BSP_MOTOR_PWM_BASE,
        kPWM_Control_Module_0 |
        kPWM_Control_Module_1 |
        kPWM_Control_Module_2,
        true);
    
    return kStatus_Success;
}

// 启动电机PWM输出
status_t BSP_MotorPwm_Start(void){
    if (!s_motorPwmInitialized){
        return kStatus_Fail;
    }
    if (!s_motorPwmRunning){
        PWM_StartTimer(BSP_MOTOR_PWM_BASE,
            kPWM_Control_Module_0 |
            kPWM_Control_Module_1 |
            kPWM_Control_Module_2);

        s_motorPwmRunning = true;
    }
    return kStatus_Success;
}
// 停止电机PWM输出
status_t BSP_MotorPwm_Stop(void){
    if (!s_motorPwmInitialized){
        return kStatus_Fail;
    }
    //先让三相占空比相同，再停止计数器
    (void)BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f);
    PWM_StopTimer(BSP_MOTOR_PWM_BASE,
        kPWM_Control_Module_0 |
        kPWM_Control_Module_1 |
        kPWM_Control_Module_2);
    s_motorPwmRunning = false;
    
    return kStatus_Success;
}