/*
 * @file bsp_p3t1755.c
 * @brief 驱动板载P3T1755温度传感器的初始化和温度读取功能。
 * 尽管官方手册上说明配置I3C通信协议需要拆除板载R55和R57电阻
 * 但拆除R55和R57只是为I3C高速通信提供电气环境，保留他们同样可以
 * 使用I3C协议配置支持I3C协议的从设备。
 * 判断通信链路是否使用I3C协议，从静态地址映射动态地址映射是否成功，
 * 数据是否从动态地址成功读取。
*/

#include "bsp/bsp_p3t1755.h"
#include "bsp/bsp_i3c.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define P3T1755_STATIC_ADDRESS 0x48U    // P3T1755芯片的静态I3C地址
#define P3T1755_DYNAMIC_ADDRESS 0x08U   // P3T1755芯片的动态I3C地址,NXP官方示例
#define P3T1755_TEMPERATURE_REGISTER 0x00U //P3T1755寄存器地址
#define P3T1755_CONFIG_REGISTER 0x01U

#define P3T1755_REGISTER_ADDRESS_SIZE 1U    // P3T1755寄存器地址长度1字节
#define P3T1755_TEMPERATURE_SET_C 0.0625f   // P3T1755温度数据的最低有效位为0.0625摄氏度

//记录温度传感器是否已经初始化
static bool s_p3t1755Initialized = false;

/*
 * 初始化板载P3T1755温度传感器。
 *
 * 函数内部会：
 * 1. 初始化I3C0；
 * 2. 清除旧的动态地址；
 * 3. 将P3T1755的静态地址0x48映射为动态地址0x08；
 * 4. 读取一次配置寄存器，确认通信正常。
 */
status_t BSP_P3T1755_Init(void){
    status_t status;
    uint8_t configRegister;

    if (s_p3t1755Initialized){
        return kStatus_Success;
    }

    status = BSP_I3C_Init();    //初始化I3C0控制器
    if (status != kStatus_Success){
        return status;
    }

    /*
     * 清除设备上一次运行时可能保留的动态地址。
     * 该操作只应在初始化阶段执行一次。避免MCU软复位后，传感器仍然保留上一次的动态地址造成SETDASA失败
     */
    status = BSP_I3C_ResetDynamicAddresses();
    if (status != kStatus_Success){
        return status;
    }

    // 将静态地址0x48映射为动态地址0x08
    status = BSP_I3C_SetDynamicAddress(
        P3T1755_STATIC_ADDRESS,
        P3T1755_DYNAMIC_ADDRESS);
    if (status != kStatus_Success){
        return status;
    }
    // 使用动态地址读取一次配置寄存器，确认地址分配成功，I3C SDR通信正常，传感器正常响应
    status = BSP_I3C_ReadRegister(P3T1755_DYNAMIC_ADDRESS, P3T1755_CONFIG_REGISTER,
        P3T1755_REGISTER_ADDRESS_SIZE, &configRegister,
        sizeof(configRegister),//1字节
        BSP_I3C_BUS_I3C_SDR);
    if (status != kStatus_Success){
        return status;
    }
    s_p3t1755Initialized = true;

    return kStatus_Success;
}
/*
 * 读取摄氏温度。
 * temperatureC： 用来接收温度数值的指针，例如25.5摄氏度返回25.5f。
 *
 * 返回值：
 *     kStatus_Success：读取成功；
 *     其他状态值：I3C通信失败或参数错误。
 * 使用方式：
 *
 * float temperature;
 *
 * if (BSP_P3T1755_ReadTemperatureC(&temperature)
 *     == kStatus_Success)
 * {
 *     // temperature中保存当前温度
 * }
 */
status_t BSP_P3T1755_ReadTemperatureQ4(int16_t *temperatureQ4){
    status_t status;
    uint8_t data[2];

    uint16_t registerValue;
    uint16_t raw12;

    if (temperatureQ4 == NULL){
        return kStatus_InvalidArgument;
    }
    // 调用者必须先调用BSP_P3T1755_Init()初始化传感器
    //初始化过程中会发送RSTDAA，清除总线所有I3C设备的动态地址，只适合在系统启动阶段执行
    if (!s_p3t1755Initialized){
        return kStatus_Fail;
    }

    status = BSP_I3C_ReadRegister(P3T1755_DYNAMIC_ADDRESS, P3T1755_TEMPERATURE_REGISTER,
        P3T1755_REGISTER_ADDRESS_SIZE, data, sizeof(data), BSP_I3C_BUS_I3C_SDR);

    if (status != kStatus_Success){
        return status;
    }
    // 拼接两个字节，data[0]是高字节，data[1]是低字节
    registerValue = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    // 温度数据位于寄存器bit15-bit4,
    raw12 = registerValue >> 4U;

    // 负温度由12位补码表示， raw12的bit11是符号位；bit11=0表示正温度，bit11=1表示负温度
    if ((raw12 & 0x0800U) != 0U){
        *temperatureQ4 = (int16_t)(raw12 | 0xF000U);
    }
    else{
        *temperatureQ4 = (int16_t)raw12;
    }

    return kStatus_Success;    
}

status_t BSP_P3T1755_ReadTemperatureC(float *temperatureC){
    status_t status;
    int16_t temperatureQ4;

    if (temperatureC == NULL){
        return kStatus_InvalidArgument;
    }

    status = BSP_P3T1755_ReadTemperatureQ4(&temperatureQ4);

    if (status != kStatus_Success){
        return status;
    }

    /*
     * 保留原浮点接口，避免破坏其他可能的调用者。
     */
    *temperatureC = (float)temperatureQ4 / 16.0f;

    return kStatus_Success;
}

