/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"

#include "bsp/bsp_i3c.h"

#define I3C_TEST_STATIC_ADDRESS  0x48U
#define I3C_TEST_DYNAMIC_ADDRESS 0x08U

#define P3T1755_CONFIG_REGISTER  0x01U

static status_t APP_I3C_SimpleTest(void)
{
    status_t status;
    uint8_t configValue = 0U;

    status = BSP_I3C_Init();

    if (status != kStatus_Success)
    {
        PRINTF("I3C init failed, status=%d\r\n",
               (int)status);
        return status;
    }

    PRINTF("I3C init success\r\n");
    PRINTF("I3C source clock: %u Hz\r\n",
           (unsigned int)BSP_I3C_GetClockFrequency());

    /*
     * 清除设备上一次运行时可能保留的动态地址。
     * 该操作只应在初始化阶段执行一次。
     */
    status = BSP_I3C_ResetDynamicAddresses();

    if (status != kStatus_Success)
    {
        PRINTF("I3C RSTDAA failed, status=%d\r\n",
               (int)status);
        return status;
    }

    /*
     * 将板载P3T1755的静态地址0x48
     * 映射为I3C动态地址0x08。
     */
    status = BSP_I3C_SetDynamicAddress(
        I3C_TEST_STATIC_ADDRESS,
        I3C_TEST_DYNAMIC_ADDRESS);

    if (status != kStatus_Success)
    {
        PRINTF("I3C SETDASA failed, status=%d\r\n",
               (int)status);
        return status;
    }

    /*
     * 使用动态地址0x08和I3C SDR模式，
     * 读取配置寄存器0x01。
     *
     * 这里并没有读取温度。
     */
    status = BSP_I3C_ReadRegister(
        I3C_TEST_DYNAMIC_ADDRESS,
        P3T1755_CONFIG_REGISTER,
        1U,
        &configValue,
        1U,
        BSP_I3C_BUS_I3C_SDR);

    if (status != kStatus_Success)
    {
        PRINTF("I3C config register read failed, status=%d\r\n",
               (int)status);
        return status;
    }

    PRINTF("I3C bus test success\r\n");
    PRINTF("P3T1755 config register: 0x%02X\r\n",
           configValue);

    return kStatus_Success;
}

int main(void)
{
    status_t i3cStatus;

    BOARD_InitHardware();

    i3cStatus = APP_I3C_SimpleTest();

    if (i3cStatus == kStatus_Success)
    {
        PRINTF("I3C test passed\r\n");
    }
    else
    {
        PRINTF("I3C test failed\r\n");
    }

    /*
     * 继续初始化OLED、按键并创建FreeRTOS任务。
     */

    vTaskStartScheduler();

    for (;;)
    {
    }
}
