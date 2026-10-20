/*
 * AS5600 driver source file
 * AS5600 0号和P3T1755使用同一个I2C端口，P3T1755工作在I3C模式下，和AS5600
 * 在地址和通信协议上均不冲突
 * ************************************************
 * AS5600为12位磁编码器。
 * 以正确对应4096个计数
 *     0       -> 0度
 *     2048    -> 180度
 *     4095    -> 接近360度
 */

#include "AS5600.h"
#include "bsp/bsp_i3c.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// AS5600使用静态7位I2C地址0x36
#define AS5600_I2C_ADDRESS 0x36U
// AS5600寄存器地址定义
#define AS5600_STATUS_REGISTER 0x0BU
#define AS5600_RAW_ANGLE_HIGH_REGISTER 0x0CU
#define AS5600_REGISTER_ADDRESS_SIZE 1U

// raw angle高字节中只有低4位属于角度数据
#define AS5600_RAW_ANGLE_HIGH_MASK 0x0FU

static bool s_as5600Initialized = false;


// AS5600属于传统I2C设备，每次访问需要明确传入BSP_I3C_BUS_I2C，让I3C0控制器产生标准I2C兼容事务
static status_t as5600_read_register(uint8_t registerAddress, uint8_t *data, size_t dataSize){
    return BSP_I3C_ReadRegister(AS5600_I2C_ADDRESS, registerAddress,
            AS5600_REGISTER_ADDRESS_SIZE, data, dataSize, BSP_I3C_BUS_I2C);
}

status_t AS5600_Init(void){
    status_t status;
    uint8_t statusRegister;
    if (s_as5600Initialized){
        return kStatus_Success;
    }
    // BSP_I3C_Init具有重复调用保护，如果P3T1755已经初始化，这里不会再次初始化
    status = BSP_I3C_Init();
    if (status != kStatus_Success){
        return status;
    }
    //读取STATUS寄存器，用于确认地址存在，AS5600是否响应，寄存器读取过程正常
    status = as5600_read_register(AS5600_STATUS_REGISTER, &statusRegister, sizeof(statusRegister));
    if (status != kStatus_Success){
        return status;
    }
    s_as5600Initialized = true;
    return kStatus_Success;
}

status_t AS5600_ReadRawAngle(uint16_t *rawAngle){
    status_t status;
    uint8_t data[2];
    if (rawAngle == NULL){
        return kStatus_InvalidArgument;
    }
    if (!s_as5600Initialized){
        return kStatus_Fail;
    }
    // 从0x0c开始连续读两个字节：data[0]：RAW_ANGLE[11:8]-data[1]：RAW_ANGLE[7:0]
    status = as5600_read_register(AS5600_RAW_ANGLE_HIGH_REGISTER, data, sizeof(data));
    if (status != kStatus_Success){
        return status;
    }
    //高字节只有低4位有效，最终结果被限制在0-4095
    *rawAngle = (uint16_t)(((uint16_t)(data[0] & AS5600_RAW_ANGLE_HIGH_MASK) << 8) | (uint16_t)data[1]);
    return kStatus_Success;
}

status_t AS5600_ReadAngleDegrees(float *angleDegrees){
    status_t status;
    uint16_t rawAngle;
    
    if (angleDegrees == NULL){
        return kStatus_InvalidArgument;
    }
    status = AS5600_ReadRawAngle(&rawAngle);
    if (status != kStatus_Success){
        return status;
    }
    // 4096个计数对应360度，rawAngle最大4095，结果不会达到360
    *angleDegrees = ((float)rawAngle) * 360.0f / AS5600_COUNTS_PER_REVOLUTION;
    return kStatus_Success;
}

status_t AS5600_ReadAngleMilliDegrees(uint32_t *angleMilliDegrees){
    status_t status;
    uint16_t rawAngle;

    if (angleMilliDegrees == NULL){
        return kStatus_InvalidArgument;
    }
    status = AS5600_ReadRawAngle(&rawAngle);
    if (status != kStatus_Success){
        return status;
    }
    // 360° = 360000毫度
    *angleMilliDegrees = ((uint32_t)rawAngle * 360000U + AS5600_COUNTS_PER_REVOLUTION / 2U) / AS5600_COUNTS_PER_REVOLUTION;

    return kStatus_Success;
}

//增加一个磁场状态检查函数，读取可靠的磁编码数据的先决条件，校准前的必要检查
/*
MD/bit 5 0x20 检测到磁铁
ML/bit 4 0x10 磁场过弱
MH/bit 3 0x08 磁场过强
*/
status_t AS5600_CheckMagnet(void){
    uint8_t status_reg = 0U;
    if (!s_as5600Initialized){
        return kStatus_Fail;
    }
    status_t ret = as5600_read_register(
        AS5600_STATUS_REGISTER, &status_reg, sizeof(status_reg)
    );
    if (ret != kStatus_Success) {
        return ret;
    }
    // MD=0，未检测到有效磁铁
    if ((status_reg & 0x20U) == 0U) {
        return kStatus_Fail;
    }
    // ML或MH置位，磁场强度异常
    if ((status_reg & 0x18U) != 0U) {
        return kStatus_Fail;
    }

    return kStatus_Success;    
}
