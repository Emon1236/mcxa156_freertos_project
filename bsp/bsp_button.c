/*
按键驱动源文件
上拉电阻接3.3V，另一侧接地，按键为低电平触发，松开为1，按下为0
*/
#include "bsp/bsp_button.h"

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_reset.h"

#include <assert.h>

/* 四个按键都使用PORT2/GPIO2 */
#define BUTTON_PORT PORT2
#define BUTTON_GPIO GPIO2

#define BUTTON_1_PIN 1U
#define BUTTON_2_PIN 2U
#define BUTTON_3_PIN 4U
#define BUTTON_4_PIN 5U
#define BUTTON_ACTIVE_LEVEL 0U

static const uint8_t s_buttonPins[BSP_BUTTON_COUNT] ={
    BUTTON_1_PIN,
    BUTTON_2_PIN,
    BUTTON_3_PIN,
    BUTTON_4_PIN
};
/* 消抖后的稳定状态：0=松开，1=按下 */
static bool s_stableState[BSP_BUTTON_COUNT];
/* 连续检测到不同状态的次数 */
static uint8_t s_debounceCounter[BSP_BUTTON_COUNT];

static bool button_read_raw(uint8_t index){
    uint32_t pinLevel;

    pinLevel = GPIO_PinRead(BUTTON_GPIO, s_buttonPins[index]);

    return (pinLevel == BUTTON_ACTIVE_LEVEL);
}

void BSP_Button_Init(void){
    CLOCK_EnableClock(kCLOCK_GatePORT2);
    CLOCK_EnableClock(kCLOCK_GateGPIO2);
    RESET_ReleasePeripheralReset(kPORT2_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO2_RST_SHIFT_RSTn);

    const port_pin_config_t buttonPortConfig = {
        kPORT_PullUp,
        kPORT_LowPullResistor,
        kPORT_SlowSlewRate,
        kPORT_PassiveFilterEnable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAsGpio,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister
    };
    const gpio_pin_config_t buttonGpioConfig ={
        kGPIO_DigitalInput,
        0U
    };
    for (uint8_t i = 0U; i < BSP_BUTTON_COUNT; i++){
        PORT_SetPinConfig(BUTTON_PORT, s_buttonPins[i], &buttonPortConfig);
        GPIO_PinInit(BUTTON_GPIO, s_buttonPins[i], &buttonGpioConfig);
        /*
         * 以启动时的真实状态作为初始稳定状态，
         * 避免上电时按住按键造成状态变量不一致。
         */
        s_stableState[i] = button_read_raw(i);
        s_debounceCounter[i] = 0U;
    }       
}

bool BSP_Button_IsPressed(bsp_button_t button){
    assert((uint32_t)button < BSP_BUTTON_COUNT);

    if ((uint32_t)button >= BSP_BUTTON_COUNT){
        return false;
    }

    return button_read_raw((uint8_t)button);
}

uint32_t BSP_Button_Scan(void){
    uint32_t pressedEvents = 0U;

    for (uint8_t i = 0U; i < BSP_BUTTON_COUNT; i++){
        bool rawState = button_read_raw(i);
        if (rawState == s_stableState[i]){
            // 当前采样与稳定状态相同，说明没有发生状态变化。   
            s_debounceCounter[i] = 0U;
        }
        else{
            // 当前采样与稳定状态不同,连续计数达到阈值后才接受新状态。
            s_debounceCounter[i]++;
            if (s_debounceCounter[i] >= BSP_BUTTON_DEBOUNCE_COUNT){
                s_debounceCounter[i] = 0U;
                s_stableState[i] = rawState;
                //*只在“稳定地变成按下状态”时产生事件,松开只更新状态，不产生按键事件。
                if (rawState){
                    pressedEvents |= (1UL << i);
                }
            }
        }
    }
    return pressedEvents;
}