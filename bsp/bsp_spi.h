/*
BSP层 SPI驱动头文件


*/

#ifndef BSP_SPI_H
#define BSP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "fsl_common.h"

// 初始化LPSPI0，时钟，复位，引脚复用，主机模式
void BSP_SPI_Init(void);

//阻塞方式发送单字节，CS仅在本次传输期间被拉低
status_t BSP_SPI_WriteByte(uint8_t data);

//阻塞方式发送一段缓冲区，CS在整个缓冲区传输期间保持拉低
status_t BSP_SPI_WriteBuffer(const uint8_t *buffer, size_t length);

//临时测试函数，发送4字节数据并接收4字节数据，返回状态码
status_t BSP_SPI_LoopbackTest(void);

#ifdef __cplusplus
} 
#endif

#endif
