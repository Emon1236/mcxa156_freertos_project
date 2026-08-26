#include "bsp/bsp_u8g2_port.h"

#include "bsp/bsp_oled_hw.h"
#include "bsp/bsp_spi.h"

#include "fsl_common.h"
#include "fsl_device_registers.h"

#include <stdbool.h>
#include <stdint.h>

static u8g2_t s_u8g2;
static bool s_u8g2Initialized;

static uint8_t bsp_u8g2_byte_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t argInt, void *argPtr){
    (void)u8x8;

    switch (msg)
    {
        case U8X8_MSG_BYTE_INIT:
            BSP_SPI_Init();
            BSP_OLED_HW_Init();
            return 1U;

        case U8X8_MSG_BYTE_START_TRANSFER:
            /*
             * 译码器没有全不选状态。
             * 每次开始传输时强制选择1号屏幕。
             */
            BSP_OLED_HW_SelectPanel(BSP_OLED_UI_PANEL);
            return 1U;

        case U8X8_MSG_BYTE_SET_DC:
            BSP_OLED_HW_SetDataMode(argInt);
            return 1U;

        case U8X8_MSG_BYTE_SEND:
            return BSP_OLED_HW_Write(
                       (const uint8_t *)argPtr,
                       argInt)
                       ? 1U
                       : 0U;

        case U8X8_MSG_BYTE_END_TRANSFER:
            /*
             * 硬件无法进入全不选状态。
             * 保持1号屏幕被选择即可。
             */
            return 1U;

        default:
            return 0U;
    }    
}

static uint8_t bsp_u8g2_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t argInt, void *argPtr){
    (void)u8x8;
    (void)argPtr;

    switch (msg)
    {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            BSP_OLED_HW_Init();
            return 1U;

        case U8X8_MSG_GPIO_RESET:
            BSP_OLED_HW_SetReset(argInt);
            return 1U;

        case U8X8_MSG_DELAY_MILLI:
            SDK_DelayAtLeastUs(
                (uint32_t)argInt * 1000U,
                SystemCoreClock);
            return 1U;

        case U8X8_MSG_DELAY_10MICRO:
            SDK_DelayAtLeastUs(
                (uint32_t)argInt * 10U,
                SystemCoreClock);
            return 1U;

        case U8X8_MSG_DELAY_100NANO:
            /*
             * SDK没有100ns级通用延时函数。
             * 初始化阶段使用1us代替，时间更长但不会破坏时序。
             */
            if (argInt != 0U)
            {
                SDK_DelayAtLeastUs(1U, SystemCoreClock);
            }
            return 1U;

        default:
            return 0U;
    }    
}

void BSP_U8G2_Init(void){
    if (s_u8g2Initialized)
    {
        return;
    }

    /*
     * 使用SSD1315、128x64、全帧缓冲模式。
     * 全缓冲需要1024字节RAM。
     */
    u8g2_Setup_ssd1315_128x64_noname_f(
        &s_u8g2,
        U8G2_R0,
        bsp_u8g2_byte_hw_spi,
        bsp_u8g2_gpio_and_delay);

    //发送SSD1315初始化命令, 此时显示器仍处于休眠状态。
    u8g2_InitDisplay(&s_u8g2);

    /*
     * 1号屏幕物理安装方向与2、3号相反。
     * U8g2的SSD1315 flip mode会发送A0和C0，
     * 与旧驱动中1号屏幕使用的方向命令一致。
     */
    u8g2_SetFlipMode(&s_u8g2, 1U);

    /* 唤醒显示器。 */
    u8g2_SetPowerSave(&s_u8g2, 0U);

    u8g2_ClearBuffer(&s_u8g2);
    u8g2_SendBuffer(&s_u8g2);

    s_u8g2Initialized = true;    
}

u8g2_t *BSP_U8G2_GetContext(void){
    return &s_u8g2;
}