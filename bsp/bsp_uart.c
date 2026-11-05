#include "bsp/bsp_uart.h"

#include "fsl_clock.h"
#include "fsl_lpuart.h"
#include "fsl_port.h"
#include "fsl_reset.h"

#include <stdbool.h>

#define BSP_UART_BASE LPUART1

static bool s_uartReady = false;

status_t BSP_UART_Init(uint32_t baudRate){
    lpuart_config_t uartConfig;
    status_t status;

    if (baudRate == 0U){
        return kStatus_InvalidArgument;
    }

    CLOCK_SetClockDiv(kCLOCK_DivLPUART1, 1U);
    CLOCK_AttachClk(kFRO12M_to_LPUART1);
    CLOCK_EnableClock(kCLOCK_GateLPUART1);
    RESET_PeripheralReset(kLPUART1_RST_SHIFT_RSTn);

    CLOCK_EnableClock(kCLOCK_GatePORT3);
    //PORT3还用于电机和蜂鸣器，只释放复位，不复位整个PORT3。
    RESET_ReleasePeripheralReset(kPORT3_RST_SHIFT_RSTn);

    const port_pin_config_t pinConfig = {
        kPORT_PullUp,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAlt3,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister
    };
    PORT_SetPinConfig(PORT3, 21U, &pinConfig); //LPUART1_TXD
    PORT_SetPinConfig(PORT3, 20U, &pinConfig); //LPUART1_RXD

    LPUART_GetDefaultConfig(&uartConfig);
    uartConfig.baudRate_Bps = baudRate;
    uartConfig.dataBitsCount = kLPUART_EightDataBits;
    uartConfig.parityMode = kLPUART_ParityDisabled;
    uartConfig.stopBitCount = kLPUART_OneStopBit;
    uartConfig.enableTx = true;
    uartConfig.enableRx = false; //本阶段只发送RPM，暂不接收命令。

    status = LPUART_Init(BSP_UART_BASE, &uartConfig, CLOCK_GetLpuartClkFreq(1U));
    s_uartReady = (status == kStatus_Success);
    return status;
}

status_t BSP_UART_Send(const uint8_t *data, size_t length){
    if (!s_uartReady){
        return kStatus_Fail;
    }
    if (data == NULL || length == 0U){
        return kStatus_InvalidArgument;
    }

    //轮询发送期间不关中断，高优先级的控制任务仍可抢占。
    return LPUART_WriteBlocking(BSP_UART_BASE, data, length);
}
