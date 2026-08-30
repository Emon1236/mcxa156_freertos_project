/*
 * 初始化板载I3C0控制器。
 *
 * 引脚：
 * SDA：P0_16
 * SCL：P0_17
 * PUR：P1_11
 */
#include "bsp/bsp_i3c.h"

#include "fsl_clock.h"
#include "fsl_i3c.h"
#include "fsl_port.h"
#include "fsl_reset.h"

#include "FreeRTOS.h"
#include "semphr.h"

//使用A156的I3C0控制器，SDA=P0_16,SCL=P0_17,PUR=P1_11
#define BSP_I3C_BASE I3C0
#define BSP_I3C_SDA_PORT PORT0
#define BSP_I3C_SDA_PIN 16U
#define BSP_I3C_SCL_PORT PORT0
#define BSP_I3C_SCL_PIN 17U
#define BSP_I3C_PUR_PORT PORT1
#define BSP_I3C_PUR_PIN 11U

// 总线速度，根据P3T1755 I3C官方示例
#define BSP_I3C_I2C_BAUDRATE_HZ  400000U
#define BSP_I3C_OD_BAUDRATE_HZ  1500000U
#define BSP_I3C_PP_BAUDRATE_HZ  4000000U

// I3C广播地址
#define BSP_I3C_BROADCAST_ADDRESS 0x7EU

//通用命令代码
#define BSP_I3C_CCC_RSTDAA 0x06U
#define BSP_I3C_CCC_SETDASA 0x87U

static bool s_i3cInitialized = false;

//等待任务的最长时间
#define BSP_I3C_MUTEX_WAIT_TICKS pdMS_TO_TICKS(10U)
//SemaphoreHandle_t是freertos管理锁的句柄，s_表示该变量只能在.c中使用，i3cBus表示保护整个I3C0物理总线
// Mutex表示该对象是FreeRTOS互斥锁
static SemaphoreHandle_t s_i3cBusMutex = NULL;

//申请I3C总线独占
static status_t bsp_i3c_take_bus(void){
    if (s_i3cBusMutex == NULL){
        return kStatus_Fail;
    }

    if (xSemaphoreTake(s_i3cBusMutex, BSP_I3C_MUTEX_WAIT_TICKS) != pdTRUE){
        return kStatus_I3C_Busy;
    }

    return kStatus_Success;
}
//释放I3C总线
static void bsp_i3c_give_bus(void){
    if (s_i3cBusMutex != NULL)
    {
        (void)xSemaphoreGive(s_i3cBusMutex);
    }
}


//将BSP层使用的总线类型转换为SDK层使用的总线类型
static status_t bsp_i3c_convert_bus_type(bsp_i3c_bus_t busType, i3c_bus_type_t *sdkBusType){
    if (sdkBusType == NULL){
        return kStatus_InvalidArgument;
    }
    switch(busType){
        case BSP_I3C_BUS_I2C:
            *sdkBusType = kI3C_TypeI2C;
            return kStatus_Success;
        case BSP_I3C_BUS_I3C_SDR:
            *sdkBusType = kI3C_TypeI3CSdr;
            return kStatus_Success;
        default:
            return kStatus_InvalidArgument;
    }
}
//配置I3C0的三个板载引脚
static void bsp_i3c_init_pins(void){
    //I3C SDA和SCL的引脚配置
    //不在PORT层强制配置开漏输出，控制器会根据当前I3C状态自动控制开漏和推挽方式
    const port_pin_config_t i3cDataClockConfig  = {
        kPORT_PullDisable,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAlt10,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister        
    };
    //P1_11连接到板载I3C上拉控制电路
    const port_pin_config_t i3cPullupControlConfig = {
        kPORT_PullDisable,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAlt10,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister        
    };
    CLOCK_EnableClock(kCLOCK_GatePORT0);
    CLOCK_EnableClock(kCLOCK_GatePORT1);


    RESET_ReleasePeripheralReset(kPORT0_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT1_RST_SHIFT_RSTn);  

    PORT_SetPinConfig(BSP_I3C_SDA_PORT, BSP_I3C_SDA_PIN, &i3cDataClockConfig);

    PORT_SetPinConfig(BSP_I3C_SCL_PORT, BSP_I3C_SCL_PIN, &i3cDataClockConfig);

    PORT_SetPinConfig(BSP_I3C_PUR_PORT, BSP_I3C_PUR_PIN, &i3cPullupControlConfig);
}

status_t BSP_I3C_Init(void){
    i3c_master_config_t masterConfig;
    uint32_t sourceClockHz;
    //总线初始化必须在创建温度和编码器任务之前
    //xSemaphoreCreateMutex()从FreeRTOS堆中创建互斥锁。如果返回NULL表示RTOS堆空间不足
    if (s_i3cBusMutex == NULL){
        s_i3cBusMutex = xSemaphoreCreateMutex();
        if (s_i3cBusMutex == NULL){
            return kStatus_Fail;
        }
    }

    if (s_i3cInitialized){
        return kStatus_Success;
    }

    /*
     * 将FRO_HF_DIV连接到I3C0功能时钟。
     * 分频配置与FRDM-MCXA156官方示例一致。
     */
    CLOCK_SetClockDiv(kCLOCK_DivI3C0_FCLK, 4U);
    CLOCK_AttachClk(kFRO_HF_DIV_to_I3C0FCLK);

    bsp_i3c_init_pins();

    sourceClockHz = CLOCK_GetI3CFClkFreq();

    if (sourceClockHz == 0U){
        return kStatus_Fail;
    }

    I3C_MasterGetDefaultConfig(&masterConfig);

    masterConfig.baudRate_Hz.i2cBaud = BSP_I3C_I2C_BAUDRATE_HZ;

    masterConfig.baudRate_Hz.i3cOpenDrainBaud = BSP_I3C_OD_BAUDRATE_HZ;

    masterConfig.baudRate_Hz.i3cPushPullBaud = BSP_I3C_PP_BAUDRATE_HZ;

    /*
     * 使用I3C推挽方式产生STOP。
     * 该设置与当前SDK的P3T1755示例一致。
     */
    masterConfig.enableOpenDrainStop = false;

    /*
     * 禁止硬件帧超时。
     * 软件驱动的等待次数由I3C_RETRY_TIMES控制。
     */
    masterConfig.disableTimeout = true;

    I3C_MasterInit(BSP_I3C_BASE, &masterConfig, sourceClockHz);

    /*
     * 初始化后总线应处于空闲状态。
     * 如果SCL或SDA被外设持续拉低，这里可能返回忙状态。
     */
    if (!I3C_MasterGetBusIdleState(BSP_I3C_BASE)){
        return kStatus_I3C_Busy;
    }

    s_i3cInitialized = true;

    return kStatus_Success;
}
 /*
 * 使用I3C0写设备寄存器。
 *
 * deviceAddress：设备的7位地址，不包含读写位。
 * registerAddress：寄存器地址。
 * registerAddressSize：寄存器地址长度，允许0~4字节。
 * data：待发送数据。
 * dataSize：待发送数据长度。
 * busType：I2C兼容模式或者I3C SDR模式。
 */
status_t BSP_I3C_WriteRegister(uint8_t deviceAddress, uint32_t registerAddress, size_t registerAddressSize, const uint8_t *data,
                                 size_t dataSize, bsp_i3c_bus_t busType){
    i3c_master_transfer_t transfer = {0};
    i3c_bus_type_t sdkBusType;
    status_t status;

    if(deviceAddress > 0x7FU){
        return kStatus_InvalidArgument;
    }
    if (registerAddressSize > 4U)
    {
        return kStatus_InvalidArgument;
    }

    if ((dataSize > 0U) && (data == NULL))
    {
        return kStatus_InvalidArgument;
    }

    if (!s_i3cInitialized)
    {
        return kStatus_Fail;
    }  
    status = bsp_i3c_convert_bus_type(busType, &sdkBusType);
    if (status != kStatus_Success){
        return status;
    }

    transfer.slaveAddress = deviceAddress;
    transfer.direction = kI3C_Write;
    transfer.busType = sdkBusType;

    transfer.subaddress = registerAddress;
    transfer.subaddressSize = registerAddressSize;
    /*
     * SDK结构体中的data是void *，写操作不会修改调用者提供的数据。
     */
    transfer.data = (void *)data;
    transfer.dataSize = dataSize;
    transfer.flags = kI3C_TransferDefaultFlag;
    //操作I3C硬件时加锁，参数检查和结构体填充都不需要占用总线，放在锁外缩短持有时间
    status = bsp_i3c_take_bus();
    if (status != kStatus_Success){
        return status;
    }

    status = I3C_MasterTransferBlocking(BSP_I3C_BASE, &transfer); 
    //不管成功失败都要释放锁
    bsp_i3c_give_bus();
    return status;
}

status_t BSP_I3C_ReadRegister(uint8_t deviceAddress, uint32_t registerAddress, size_t registerAddressSize, uint8_t *data,
                              size_t dataSize, bsp_i3c_bus_t busType){
    i3c_master_transfer_t transfer = {0};
    i3c_bus_type_t sdkBusType;
    status_t status;
    if (deviceAddress > 0x7FU)
    {
        return kStatus_InvalidArgument;
    }

    if (registerAddressSize > 4U)
    {
        return kStatus_InvalidArgument;
    }

    if ((dataSize > 0U) && (data == NULL))
    {
        return kStatus_InvalidArgument;
    }

    if (!s_i3cInitialized)
    {
        return kStatus_Fail;
    }
    status = bsp_i3c_convert_bus_type(busType, &sdkBusType);
    if (status != kStatus_Success){
        return status;
    }

    transfer.slaveAddress = deviceAddress;
    transfer.direction = kI3C_Read;
    transfer.busType = sdkBusType;

    transfer.subaddress = registerAddress;
    transfer.subaddressSize = registerAddressSize;

    transfer.data = data;
    transfer.dataSize = dataSize;
    transfer.flags = kI3C_TransferDefaultFlag;

    status = bsp_i3c_take_bus();
    if (status != kStatus_Success){
        return status;
    }

    status = I3C_MasterTransferBlocking(BSP_I3C_BASE, &transfer);
    bsp_i3c_give_bus();
    return status;
}
/*
 * 发送广播CCC命令RSTDAA，
 * 清除总线上所有I3C设备的动态地址。
 *
 * 这个函数会影响总线上所有I3C设备，
 * 正常情况下只在系统初始化阶段调用一次。
 */
status_t BSP_I3C_ResetDynamicAddresses(void){
    uint8_t command = BSP_I3C_CCC_RSTDAA;

    /*
     * RSTDAA是广播CCC命令。
     *
     * 向广播地址0x7E发送一个字节0x06，
     * 清除总线上所有I3C设备的动态地址。
     */
    return BSP_I3C_WriteRegister(BSP_I3C_BROADCAST_ADDRESS, 0U, 0U, &command, 1U, BSP_I3C_BUS_I3C_SDR);
}

/*
 * 使用SETDASA命令，根据设备静态地址设置动态地址。
 *
 * staticAddress ：设备原始静态地址。
 * dynamicAddress：准备分配的I3C动态地址。
 * 由两个连续传输组成，中间不能被AS5600任务插入，所以要一次性持有锁。
 */
status_t BSP_I3C_SetDynamicAddress(uint8_t staticAddress, uint8_t dynamicAddress){
    i3c_master_transfer_t transfer = {0};
    uint8_t command;
    uint8_t encodedDynamicAddress;
    status_t status;

    if (!s_i3cInitialized){
        return kStatus_Fail;
    }

    if ((staticAddress == 0U) || (staticAddress >= BSP_I3C_BROADCAST_ADDRESS) || (dynamicAddress == 0U) ||
        (dynamicAddress >= BSP_I3C_BROADCAST_ADDRESS)){
        return kStatus_InvalidArgument;
    }

    /*
     * 第一步：
     * 向广播地址0x7E发送SETDASA命令。
     *
     * 此处不能产生STOP，因为后面还要继续发送
     * 目标设备的静态地址和新动态地址。
     */
    status = bsp_i3c_take_bus();
    if (status != kStatus_Success){
        return status;
    }
    command = BSP_I3C_CCC_SETDASA;

    transfer.slaveAddress = BSP_I3C_BROADCAST_ADDRESS;
    transfer.direction = kI3C_Write;
    transfer.busType = kI3C_TypeI3CSdr;
    transfer.data = &command;
    transfer.dataSize = 1U;
    transfer.flags = kI3C_TransferNoStopFlag;

    status = I3C_MasterTransferBlocking(BSP_I3C_BASE, &transfer);

    if (status != kStatus_Success){
        bsp_i3c_give_bus();
        return status;
    }

    /*
     * 第二步：
     * 使用设备静态地址选中设备，
     * 然后发送左移一位后的动态地址。
     */
    transfer = (i3c_master_transfer_t){0};

    encodedDynamicAddress = (uint8_t)(dynamicAddress << 1U);

    transfer.slaveAddress = staticAddress;
    transfer.direction = kI3C_Write;
    transfer.busType = kI3C_TypeI3CSdr;
    transfer.data = &encodedDynamicAddress;
    transfer.dataSize = 1U;
    transfer.flags = kI3C_TransferDefaultFlag;
    
    status = I3C_MasterTransferBlocking(BSP_I3C_BASE, &transfer);
    bsp_i3c_give_bus();
    return status;
}
/* 返回当前I3C0功能时钟频率，单位Hz。 */
uint32_t BSP_I3C_GetClockFrequency(void){
    return CLOCK_GetI3CFClkFreq();
}
