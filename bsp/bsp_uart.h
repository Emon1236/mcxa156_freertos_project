#ifndef BSP_UART_H
#define BSP_UART_H

#include "fsl_common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 蓝牙串口LPUART1：P3_21为TX，P3_20为RX，8N1。 */
status_t BSP_UART_Init(uint32_t baudRate);
/* 按指定字节数发送二进制数据，仅在任务中调用。 */
status_t BSP_UART_Send(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
