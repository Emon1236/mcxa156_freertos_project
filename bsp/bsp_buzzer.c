/*
蜂鸣器驱动源文件
MLT5020是无源蜂鸣器，需要用PWM信号驱动，单纯的高低电平变化不能发声，
可通过pwm信号的频率发出不同音调的声音。在接近4 kHz的频率下驱动时，蜂鸣器通常能够获得较大的响度。
蜂鸣器驱动引脚配置P3_17
*/
#include "bsp/bsp_buzzer.h"

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_reset.h"
#include "fsl_pwm.h"

#include <stdbool.h>
// 蜂鸣器控制引脚：P3_17
#define BUZZER_PORT        PORT3
#define BUZZER_GPIO        GPIO3
#define BUZZER_PIN         17U

#define BUZZER_PWM_BASE      FLEXPWM1
#define BUZZER_PWM_MODULE    kPWM_Module_0
#define BUZZER_PWM_CHANNEL   kPWM_PwmB
#define BUZZER_PWM_MASK       kPWM_Control_Module_0

#define BUZZER_PWM_DIVIDER   32U

static bool s_initialized = false;
static uint32_t s_sourceClockHz = 0U;

// 初始化蜂鸣器驱动引脚。
status_t BSP_Buzzer_Init(void){
    pwm_config_t pwmConfig;
    status_t status;

    const gpio_pin_config_t gpioConfig = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 0U
    };
    const port_pin_config_t pinConfig = {
        .pullSelect = kPORT_PullDown,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .driveStrength1 = kPORT_NormalDriveStrength,
        .mux = kPORT_MuxAsGpio,
        .inputBuffer = kPORT_InputBufferDisable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister
    };
    if (s_initialized){
        return kStatus_Success;
    }
    CLOCK_EnableClock(kCLOCK_GatePORT3);
    CLOCK_EnableClock(kCLOCK_GateGPIO3);
    //PORT3也被电机任务使用，只释放复位，不对整个PORT3做外设复位
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO3_RST_SHIFT_RSTn);    
    //准备GPIO低电平，再连接到引脚
    GPIO_PinInit(BUZZER_GPIO, BUZZER_PIN, &gpioConfig);
    PORT_SetPinConfig(BUZZER_PORT, BUZZER_PIN, &pinConfig);

    CLOCK_EnableClock(kCLOCK_GateFLEXPWM1);
    RESET_ReleasePeripheralReset(kFLEXPWM1_RST_SHIFT_RSTn);
    //使用MainClk作为源频率，96MHz
    s_sourceClockHz = CLOCK_GetFreq(kCLOCK_MainClk);
    if (s_sourceClockHz == 0U){
        return kStatus_Fail;
    }    
    PWM_GetDefaultConfig(&pwmConfig);

    pwmConfig.clockSource = kPWM_BusClock;
    pwmConfig.prescale = kPWM_Prescale_Divide_32;
    pwmConfig.pairOperation = kPWM_Independent;
    pwmConfig.initializationControl = kPWM_Initialize_LocalSync;
    pwmConfig.reloadSelect = kPWM_LocalReload;

    /* 本方案只在停止输出后修改频率，使用立即装载。 */
    pwmConfig.reloadLogic = kPWM_ReloadImmediate;
    pwmConfig.enableDebugMode = false;

    status = PWM_Init(BUZZER_PWM_BASE, BUZZER_PWM_MODULE, &pwmConfig);
    if (status != kStatus_Success){
        return status;
    }
    PWM_OutputDisable(BUZZER_PWM_BASE, BUZZER_PWM_CHANNEL, BUZZER_PWM_MODULE);
    PWM_StopTimer(BUZZER_PWM_BASE, BUZZER_PWM_MASK);
    /*
     * 蜂鸣器没有使用 PWM FAULT 输入。
     * 只解除PWM1子模块0B通道的故障映射。
     * 不修改电机PWM0的故障配置。
     */
    PWM_SetupFaultDisableMap(BUZZER_PWM_BASE, BUZZER_PWM_MODULE, BUZZER_PWM_CHANNEL,
                            kPWM_faultchannel_0, 0U);

    s_initialized = true;
    return kStatus_Success;
}

void BSP_Buzzer_Stop(void){
    if(!s_initialized){
        return;
    }
    //先拉低，保证关闭后引脚是确定的低电平
    GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, 0U);
    PORT_SetPinMux(BUZZER_PORT, BUZZER_PIN, kPORT_MuxAsGpio);

    PWM_OutputDisable(BUZZER_PWM_BASE, BUZZER_PWM_CHANNEL, BUZZER_PWM_MODULE);
    PWM_StopTimer(BUZZER_PWM_BASE, BUZZER_PWM_MASK);    
}

status_t BSP_Buzzer_Start(uint32_t frequencyHz){
    pwm_signal_param_t signal = {0};
    uint32_t periodTicks;
    status_t status;

    if (!s_initialized){
        return kStatus_Fail;
    }
    /* 改频率前先停止旧音。 */
    BSP_Buzzer_Stop();
    if ((frequencyHz < BSP_BUZZER_MIN_FREQUENCY_HZ) ||
        (frequencyHz > BSP_BUZZER_MAX_FREQUENCY_HZ)){
        return kStatus_InvalidArgument;
    }

    periodTicks = (s_sourceClockHz / BUZZER_PWM_DIVIDER) / frequencyHz;

    if ((periodTicks < 2U) || (periodTicks > 65535U)){
        return kStatus_InvalidArgument;
    }

    signal.pwmChannel = BUZZER_PWM_CHANNEL;
    signal.level = kPWM_HighTrue;
    signal.dutyCyclePercent = 50U;
    signal.deadtimeValue = 0U;
    signal.faultState = kPWM_PwmFaultState0;

    /* 配置期间先不开放输出。 */
    signal.pwmchannelenable = false;

    PWM_SetPwmLdok(BUZZER_PWM_BASE, BUZZER_PWM_MASK, false);

    status = PWM_SetupPwm(BUZZER_PWM_BASE, BUZZER_PWM_MODULE, &signal,
                         1U, kPWM_EdgeAligned, frequencyHz, s_sourceClockHz);
    if (status != kStatus_Success){
        return status;
    }

    PWM_SetPwmLdok(BUZZER_PWM_BASE, BUZZER_PWM_MASK, true);

    PWM_OutputEnable(BUZZER_PWM_BASE, BUZZER_PWM_CHANNEL, BUZZER_PWM_MODULE);
    PWM_StartTimer(BUZZER_PWM_BASE, BUZZER_PWM_MASK);

    /* P3_17 ALT7：PWM1_B0。 */
    PORT_SetPinMux(BUZZER_PORT, BUZZER_PIN, kPORT_MuxAlt7);

    return kStatus_Success;
}
