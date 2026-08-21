#ifndef BSP_I3C_H
#define BSP_I3C_H

#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
I3C控制器支持两种传输方式
BSP_I3C_BUS_I2C:
    使用I2C兼容模式访问传统I2C设备
BSP_I3C_BUS_I3C:
    使用I3C SDR模式访问已经分配动态地址的I3C设备
*/
typedef enum{
    BSP_I3C_BUS_I2C = 0U,
    BSP_I3C_BUS_I3C_SDR
}bsp_i3c_bus_t;

status_t BSP_I3C_Init(void);

status_t BSP_I3C_WriteRegister(uint8_t deviceAddress, uint32_t registerAddress, size_t registerAddressSize, const uint8_t *data, size_t dataSize, bsp_i3c_bus_t busType);
status_t BSP_I3C_ReadRegister(uint8_t deviceAddress, uint32_t registerAddress, size_t registerAddressSize, uint8_t *data, size_t dataSize, bsp_i3c_bus_t busType);

status_t BSP_I3C_ResetDynamicAddresses(void);
status_t BSP_I3C_SetDynamicAddress(uint8_t staticAddress, uint8_t dynamicAddress);
uint32_t BSP_I3C_GetClockFrequency(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_I3C_H */