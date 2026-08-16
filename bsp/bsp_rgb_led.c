/*
BSP层 RGB驱动源文件
根据board.h中的定义，RGB LED GPIO输出0为点亮，输出1为熄灭
RGB LED定义引脚：
     * Red   GPIO3[12]
     * Green GPIO3[13]
     * Blue  GPIO3[0]

问题：LED初始化缺少时钟使能和复位释放，导致LED无法点亮。
NXP官方的led_output示例中，包含GPIO3时钟使能和GPIO3/PORT3复位释放，参考如下：
    CLOCK_EnableClock(kCLOCK_GateGPIO3);
    CLOCK_EnableClock(kCLOCK_GatePORT3);
    RESET_ReleasePeripheralReset(kGPIO3_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);
MCX-A系列外设默认处于复位状态，不是放复位，寄存器写入可能完全不生效。
*/

#include "bsp/bsp_rgb_led.h"
#include "board.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_reset.h"

static void rgb_led_write(GPIO_Type *gpio, uint32_t pin, uint8_t value){
    GPIO_PinWrite(gpio, pin, value);
}

void BSP_RGB_LED_Init(void){
    gpio_pin_config_t led_config = {
        kGPIO_DigitalOutput,
        LOGIC_LED_OFF
    };
    CLOCK_EnableClock(kCLOCK_GateGPIO3);//打开GPIO3时钟
    CLOCK_EnableClock(kCLOCK_GatePORT3);//打开Port3时钟
    RESET_ReleasePeripheralReset(kGPIO3_RST_SHIFT_RSTn);//释放GPIO3复位
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);//释放PORT3复位
    PORT_SetPinMux(PORT3, BOARD_LED_RED_GPIO_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORT3, BOARD_LED_GREEN_GPIO_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORT3, BOARD_LED_BLUE_GPIO_PIN, kPORT_MuxAsGpio);//设置为GPIO功能
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &led_config);
    GPIO_PinInit(BOARD_LED_GREEN_GPIO, BOARD_LED_GREEN_GPIO_PIN, &led_config);
    GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, &led_config);
    BSP_RGB_LED_Off();    
}

void BSP_RGB_LED_Off(void){
    rgb_led_write(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, LOGIC_LED_OFF);
    rgb_led_write(BOARD_LED_GREEN_GPIO, BOARD_LED_GREEN_GPIO_PIN, LOGIC_LED_OFF);
    rgb_led_write(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, LOGIC_LED_OFF);
}

void BSP_RGB_SetColor(bsp_rgb_led_t color){
    BSP_RGB_LED_Off();
    switch(color){
        case BSP_RGB_COLOR_RED:
            rgb_led_write(BOARD_LED_RED_GPIO,BOARD_LED_RED_GPIO_PIN,LOGIC_LED_ON);
            break;
        case BSP_RGB_COLOR_GREEN:
            rgb_led_write(BOARD_LED_GREEN_GPIO,BOARD_LED_GREEN_GPIO_PIN,LOGIC_LED_ON);
            break;
        case BSP_RGB_COLOR_BLUE:
            rgb_led_write(BOARD_LED_BLUE_GPIO,BOARD_LED_BLUE_GPIO_PIN,LOGIC_LED_ON);
            break;
        case BSP_RGB_COLOR_OFF:
        default:
            break;
    }
}

