/*
蜂鸣器驱动源文件
MLT5020是无源蜂鸣器，需要用PWM信号驱动，单纯的高低电平变化不能发声，
可通过pwm信号的频率发出不同音调的声音。在接近4 kHz的频率下驱动时，蜂鸣器通常能够获得较大的响度。
蜂鸣器驱动引脚配置P2_6
*/
#include "bsp/bsp_buzzer.h"

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_reset.h"
// 蜂鸣器控制引脚：P2_6
#define BUZZER_PORT        PORT2
#define BUZZER_GPIO        GPIO2
#define BUZZER_PIN         6U

#define BUZZER_ACTIVE_LEVEL 1U
#define BUZZER_IDLE_LEVEL   0U

#define BUZZER_MIN_FREQUENCY_HZ 50U
#define BUZZER_MAX_FREQUENCY_HZ 10000U
// 初始化蜂鸣器驱动引脚。
void BSP_Buzzer_Init(void){
    const port_pin_config_t buzzerPortConfig = {
        kPORT_PullDown,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAsGpio,
        kPORT_InputBufferDisable,
        kPORT_InputNormal,
        kPORT_UnlockRegister
    };
    //P2_6配置为数字输出，初始输出空闲电平
    const gpio_pin_config_t buzzerGpioConfig = {kGPIO_DigitalOutput,BUZZER_IDLE_LEVEL};

    CLOCK_EnableClock(kCLOCK_GatePORT2);
    CLOCK_EnableClock(kCLOCK_GateGPIO2);//开外设时钟
    RESET_ReleasePeripheralReset(kPORT2_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO2_RST_SHIFT_RSTn);//释放外设复位

    PORT_SetPinConfig(BUZZER_PORT, BUZZER_PIN, &buzzerPortConfig); //配置引脚复用和GPIO输出方向
    GPIO_PinInit(BUZZER_GPIO, BUZZER_PIN, &buzzerGpioConfig);       //
    BSP_Buzzer_Stop(); //初始化时关闭蜂鸣器
}

//源文件内部函数，用static修饰
static void buzzer_set_active(uint8_t active){
    if (active == 1U){
        GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, BUZZER_ACTIVE_LEVEL);
    } else {
        GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, BUZZER_IDLE_LEVEL);
    }
}
void BSP_Buzzer_Stop(void){
    buzzer_set_active(0U);
}
//GPIO软件翻转产生指定频率方波，不适用于freertos任务中使用，会阻塞任务调度
void BSP_Buzzer_PlayTone(uint32_t frequencyHz, uint32_t durationMs){
    uint32_t halfPeriodUs;
    uint32_t periodCount;

    if ((frequencyHz < BUZZER_MIN_FREQUENCY_HZ) ||
        (frequencyHz > BUZZER_MAX_FREQUENCY_HZ) ||
        (durationMs == 0U)){
        BSP_Buzzer_Stop();
        return;
    }
    /*
    halfPeriodUs是指方波高或低电平持续时间，单位us，如果传入4kHz,1s要产生4k个周期
    1个周期=1/4000=250us，高电平持续125us，低电平持续125us，所以这里是500000us，计算得到125us
    periodCount指要产生多少个周期，4kHz一秒是4000个周期，如果持续时间30ms，
    30/1000 = periodCount/4000，计算得到periodCount = 120个周期
    */
    halfPeriodUs = 500000U / frequencyHz;
    periodCount = (uint32_t)(((uint64_t)frequencyHz * durationMs) / 1000U);

    if (periodCount == 0U){
        periodCount = 1U;
    }

    for (uint32_t i = 0U; i < periodCount; i++){
        buzzer_set_active(1U);
        SDK_DelayAtLeastUs(halfPeriodUs, SystemCoreClock);

        buzzer_set_active(0U);
        SDK_DelayAtLeastUs(halfPeriodUs, SystemCoreClock);
    }

    BSP_Buzzer_Stop();
}
