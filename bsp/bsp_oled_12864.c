/*
SSD1315 OLED12864 x4 集成屏幕驱动，SPI总线共享，通过A0/A1和译码器选屏
RES/D-C复用原SDI/CS引脚；A0/A1为新增GPIO

*----------------------连线说明---------------------
VCC-3.3V                        RES-SDI(P1_2)
GND-GND                         D/C-CS(P1_3)
D0(SCK)-SCK(P1_1)               A0-P1_13
D1(MOSI)-SDO(P1_0)              A1-P1_12
*---------------------------------------------------

*----------------------片段地址映射表----------------
A0:A1           译码器Y0~Y3         硬件对应
0:0             0111                0号屏(左上角) 
0:1             1011                1号屏(右上角)
1:0             1101                2号屏(左下角)
1:1             1110                3号屏(右下角)
*---------------------------------------------------

测试说明：0号屏幕顶层1/3区域存在坏点，按当前硬件分布，镜像后为底部1/3区域失能
*/
#include "bsp/bsp_oled_12864.h"
#include "bsp/bsp_oled_data.h"
#include "bsp/bsp_spi.h"
#include "board.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_reset.h"
#include <string.h>

/* RES: 原SDI(P1_2)，D/C: 原CS/PCS0(P1_3) */
#define OLED_RES_GPIO GPIO1
#define OLED_RES_PORT PORT1
#define OLED_RES_PIN  2U
#define OLED_DC_GPIO  GPIO1
#define OLED_DC_PORT  PORT1
#define OLED_DC_PIN   3U

/* A0: -> P1_13, A1: -> P1_12 */
#define OLED_A0_GPIO  GPIO1
#define OLED_A0_PORT  PORT1
#define OLED_A0_PIN   13U
#define OLED_A1_GPIO  GPIO1
#define OLED_A1_PORT  PORT1
#define OLED_A1_PIN   12U

/* 先只测试单屏，帧缓冲复用；后续扩展4屏可改成 s_oledBuffer[OLED_PANEL_COUNT][...] */
static uint8_t s_oledBuffer[OLED_HEIGHT / 8U][OLED_WIDTH];

static void oled_gpio_pin_init(void)
{
    CLOCK_EnableClock(kCLOCK_GatePORT1);
    CLOCK_EnableClock(kCLOCK_GatePORT2);
    CLOCK_EnableClock(kCLOCK_GatePORT3);
    CLOCK_EnableClock(kCLOCK_GateGPIO1);
    CLOCK_EnableClock(kCLOCK_GateGPIO2);
    CLOCK_EnableClock(kCLOCK_GateGPIO3);
    RESET_ReleasePeripheralReset(kPORT1_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT2_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO1_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO2_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kGPIO3_RST_SHIFT_RSTn);

    const port_pin_config_t gpio_pin_config = {
        kPORT_PullUp, kPORT_LowPullResistor, kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength, kPORT_NormalDriveStrength,
        kPORT_MuxAsGpio, kPORT_InputBufferEnable, kPORT_InputNormal,
        kPORT_UnlockRegister
    };
    PORT_SetPinConfig(OLED_RES_PORT, OLED_RES_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_DC_PORT, OLED_DC_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_A0_PORT, OLED_A0_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_A1_PORT, OLED_A1_PIN, &gpio_pin_config);

    gpio_pin_config_t out_config = {kGPIO_DigitalOutput, 1U};
    GPIO_PinInit(OLED_RES_GPIO, OLED_RES_PIN, &out_config);
    GPIO_PinInit(OLED_DC_GPIO, OLED_DC_PIN, &out_config);
    GPIO_PinInit(OLED_A0_GPIO, OLED_A0_PIN, &out_config);
    GPIO_PinInit(OLED_A1_GPIO, OLED_A1_PIN, &out_config);
}

void BSP_OLED_SelectPanel(uint8_t index)
{
    /* 译码表: A0:A1=00->Y0 01->Y1 10->Y2 11->Y3，index即(A0<<1)|A1 */
    GPIO_PinWrite(OLED_A0_GPIO, OLED_A0_PIN, (index >> 1U) & 0x01U);
    GPIO_PinWrite(OLED_A1_GPIO, OLED_A1_PIN, index & 0x01U);
}

static void oled_write_command(uint8_t cmd)
{
    GPIO_PinWrite(OLED_DC_GPIO, OLED_DC_PIN, 0U);
    BSP_SPI_WriteByte(cmd);
}


static void oled_reset(void)
{
    /* RES为4屏共享线，复位时会同时复位所有屏 */
    GPIO_PinWrite(OLED_RES_GPIO, OLED_RES_PIN, 0U);
    for (volatile uint32_t i = 0U; i < 100000U; i++) {}
    GPIO_PinWrite(OLED_RES_GPIO, OLED_RES_PIN, 1U);
    for (volatile uint32_t i = 0U; i < 100000U; i++) {}
}

void BSP_OLED_Init(uint8_t index)
{
    oled_gpio_pin_init();
    oled_reset();
    BSP_OLED_SelectPanel(index);

    oled_write_command(0xAE);
    oled_write_command(0xD5); oled_write_command(0x80);
    oled_write_command(0xA8); oled_write_command(0x3F);
    oled_write_command(0xD3); oled_write_command(0x00);
    oled_write_command(0x40);
    oled_write_command(0x8D); oled_write_command(0x14);
    oled_write_command(0x20); oled_write_command(0x00);
    oled_write_command(0xA1);
    oled_write_command(0xC8);
    oled_write_command(0xDA); oled_write_command(0x12);
    oled_write_command(0x81); oled_write_command(0xCF);
    oled_write_command(0xD9); oled_write_command(0xF1);
    oled_write_command(0xDB); oled_write_command(0x40);
    oled_write_command(0xA4);
    oled_write_command(0xA6);
    oled_write_command(0xAF);

    BSP_OLED_Clear();
    BSP_OLED_Refresh();
}

void BSP_OLED_Clear(void)
{
    memset(s_oledBuffer, 0, sizeof(s_oledBuffer));
}

void BSP_OLED_Refresh(void)
{
    for (uint8_t page = 0U; page < (OLED_HEIGHT / 8U); page++)
    {
        oled_write_command(0xB0U + page);
        oled_write_command(0x00);
        oled_write_command(0x10);
        GPIO_PinWrite(OLED_DC_GPIO, OLED_DC_PIN, 1U);
        BSP_SPI_WriteBuffer(s_oledBuffer[page], OLED_WIDTH);
    }
}

void BSP_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t fill)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
    {
        return;
    }
    uint8_t page = y / 8U;
    uint8_t bit  = y % 8U;
    if (fill)
    {
        s_oledBuffer[page][x] |= (uint8_t)(1U << bit);
    }
    else
    {
        s_oledBuffer[page][x] &= (uint8_t)~(1U << bit);
    }
}

void BSP_OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    int16_t dx  = (int16_t)x1 - (int16_t)x0;
    int16_t dy  = (int16_t)y1 - (int16_t)y0;
    int16_t sx  = (dx > 0) ? 1 : -1;
    int16_t sy  = (dy > 0) ? 1 : -1;
    int16_t adx = (dx > 0) ? dx : -dx;
    int16_t ady = (dy > 0) ? dy : -dy;
    int16_t err = adx - ady;
    int16_t x   = (int16_t)x0;
    int16_t y   = (int16_t)y0;

    while (1)
    {
        BSP_OLED_DrawPoint((uint8_t)x, (uint8_t)y, 1U);
        if (x == x1 && y == y1)
        {
            break;
        }
        int16_t e2 = err * 2;
        if (e2 > -ady) { err -= ady; x += sx; }
        if (e2 < adx)  { err += adx; y += sy; }
    }
}

void BSP_OLED_ShowChar(uint8_t x, uint8_t y, char ch)
{
    for (uint8_t i = 0U; i < g_asciiFont6x8Count; i++)
    {
        if (g_asciiFont6x8[i].ch == ch)
        {
            for (uint8_t col = 0U; col < 6U; col++)
            {
                uint8_t colData = g_asciiFont6x8[i].data[col];
                for (uint8_t row = 0U; row < 8U; row++)
                {
                    BSP_OLED_DrawPoint((uint8_t)(x + col), (uint8_t)(y + row),
                                       (colData >> row) & 0x01U);
                }
            }
            return;
        }
    }
}

void BSP_OLED_ShowString(uint8_t x, uint8_t y, const char *str)
{
    uint8_t cursor = x;
    while (*str != '\0')
    {
        BSP_OLED_ShowChar(cursor, y, *str);
        cursor = (uint8_t)(cursor + 6U);
        str++;
    }
}

void BSP_OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num)
{
    char buf[10];
    uint8_t len = 0U;

    if (num == 0U)
    {
        buf[len++] = '0';
    }
    while (num > 0U && len < sizeof(buf))
    {
        buf[len++] = (char)('0' + (num % 10U));
        num /= 10U;
    }
    /* 数字是逆序生成的，从右往左显示实现从左到右正确顺序 */
    for (uint8_t i = 0U; i < len; i++)
    {
        BSP_OLED_ShowChar((uint8_t)(x + (len - 1U - i) * 6U), y, buf[i]);
    }
}

void BSP_OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t fontIndex)
{
    const uint8_t *data = g_chineseFont16x16[fontIndex].data;
    for (uint8_t row = 0U; row < 16U; row++)
    {
        uint8_t high = data[row * 2U];
        uint8_t low  = data[row * 2U + 1U];
        for (uint8_t col = 0U; col < 8U; col++)
        {
            BSP_OLED_DrawPoint((uint8_t)(x + col), (uint8_t)(y + row), (high >> (7U - col)) & 0x01U);
            BSP_OLED_DrawPoint((uint8_t)(x + 8U + col), (uint8_t)(y + row), (low >> (7U - col)) & 0x01U);
        }
    }
}