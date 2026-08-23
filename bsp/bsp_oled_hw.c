#include "bsp/bsp_oled_hw.h"

#include "bsp/bsp_spi.h"

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_reset.h"

#define OLED_RES_GPIO GPIO1
#define OLED_RES_PORT PORT1
#define OLED_RES_PIN  2U

#define OLED_DC_GPIO  GPIO1
#define OLED_DC_PORT  PORT1
#define OLED_DC_PIN   3U

#define OLED_A0_GPIO  GPIO1
#define OLED_A0_PORT  PORT1
#define OLED_A0_PIN   13U

#define OLED_A1_GPIO  GPIO1
#define OLED_A1_PORT  PORT1
#define OLED_A1_PIN   12U


static bool s_hardwareInitialized;

void BSP_OLED_HW_Init(void)
{
    const port_pin_config_t portConfig = {
        kPORT_PullUp,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAsGpio,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister
    };

    const gpio_pin_config_t outputConfig = {
        kGPIO_DigitalOutput,
        1U
    };

    if (s_hardwareInitialized)
    {
        return;
    }

    CLOCK_EnableClock(kCLOCK_GateGPIO1);
    CLOCK_EnableClock(kCLOCK_GatePORT1);

    RESET_ReleasePeripheralReset(kGPIO1_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT1_RST_SHIFT_RSTn);

    PORT_SetPinConfig(OLED_RES_PORT, OLED_RES_PIN, &portConfig);
    PORT_SetPinConfig(OLED_DC_PORT, OLED_DC_PIN, &portConfig);
    PORT_SetPinConfig(OLED_A0_PORT, OLED_A0_PIN, &portConfig);
    PORT_SetPinConfig(OLED_A1_PORT, OLED_A1_PIN, &portConfig);

    GPIO_PinInit(OLED_RES_GPIO, OLED_RES_PIN, &outputConfig);
    GPIO_PinInit(OLED_DC_GPIO, OLED_DC_PIN, &outputConfig);
    GPIO_PinInit(OLED_A0_GPIO, OLED_A0_PIN, &outputConfig);
    GPIO_PinInit(OLED_A1_GPIO, OLED_A1_PIN, &outputConfig);

    BSP_OLED_HW_SelectPanel(BSP_OLED_UI_PANEL);
    BSP_OLED_HW_SetReset(1U);
    BSP_OLED_HW_SetDataMode(1U);

    s_hardwareInitialized = true;
}

void BSP_OLED_HW_SelectPanel(uint8_t panel)
{
    if (panel >= BSP_OLED_PANEL_COUNT)
    {
        return;
    }

    GPIO_PinWrite(
        OLED_A0_GPIO,
        OLED_A0_PIN,
        (panel >> 1U) & 0x01U);

    GPIO_PinWrite(
        OLED_A1_GPIO,
        OLED_A1_PIN,
        panel & 0x01U);
}

void BSP_OLED_HW_SetReset(uint8_t level)
{
    GPIO_PinWrite(
        OLED_RES_GPIO,
        OLED_RES_PIN,
        level ? 1U : 0U);
}

void BSP_OLED_HW_SetDataMode(uint8_t isData)
{
    GPIO_PinWrite(
        OLED_DC_GPIO,
        OLED_DC_PIN,
        isData ? 1U : 0U);
}

bool BSP_OLED_HW_Write(const uint8_t *data, size_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return false;
    }

    return BSP_SPI_WriteBuffer(data, length) == kStatus_Success;
}