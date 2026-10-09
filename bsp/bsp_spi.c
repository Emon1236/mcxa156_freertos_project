/*
BSP层 SPI驱动源文件
基于LPSPI0，仅实现主机发送(OLED/SSD1315等只写设备使用)
引脚定义(FRDM-MCXA156)：
     * SCK   P1_1  (pin 92)  LPSPI0_SCK
     * SDO   P1_0  (pin 91)  LPSPI0_SDO (MOSI)
     * PCS0  P1_3  (pin 94)  LPSPI0_PCS0 (CS，由LPSPI硬件自动控制)
*/

#include "bsp/bsp_spi.h"
#include "board.h"
#include "fsl_lpspi.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_reset.h"
#include "fsl_debug_console.h"
#include <string.h>

#define BSP_SPI_MASTER_BASEADDR         LPSPI0
#define BSP_SPI_MASTER_PCS_FOR_INIT     kLPSPI_Pcs0
#define BSP_SPI_MASTER_PCS_FOR_TRANSFER kLPSPI_MasterPcs0
#define BSP_SPI_MASTER_BAUDRATE         4000000U /* 4MHz，SSD1315可正常识别，后续可按需调高 */

//诊断变量，仅供调试器查看
typedef struct
{
    uint32_t callIndex;   /* 第几次发送 */
    uint32_t length;      /* 本次请求发送的字节数 */
    status_t status;     /* SDK 返回状态 */

    uint32_t sr;         /* 原始状态寄存器 */
    uint32_t fsr;        /* 原始 FIFO 状态寄存器 */

    uint32_t txCount;    /* 发送 FIFO 中的条目数量 */
    uint32_t mbf;        /* 1：模块仍忙 */
    uint32_t tcf;        /* 1：传输完成标志已置位 */
} bsp_spi_diag_sample_t;

//volatile让每次的记录写入RAM，方便调试器观察；static变量值属于本文件，但具有整个程序运行期间的生命周期
static volatile uint32_t s_spiDiagCalls = 0U;
static volatile uint32_t s_spiDiagErrors = 0U;
static volatile uint32_t s_spiDiagSuccessPending = 0U;
/* 最近一次发送的快照。 */
static volatile bsp_spi_diag_sample_t s_spiDiagLast = {0};

/* 第一次“返回成功但仍有数据待发送”的快照，之后不覆盖。 */
static volatile bsp_spi_diag_sample_t s_spiDiagFirstPending = {0};

static void bsp_spi_pin_init(void){
    CLOCK_EnableClock(kCLOCK_GatePORT1); //时钟使能,port1
    RESET_ReleasePeripheralReset(kPORT1_RST_SHIFT_RSTn); //时钟复位释放,port1

    const port_pin_config_t spi_pin_config = {
        kPORT_PullDisable,
        kPORT_LowPullResistor,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength,
        kPORT_NormalDriveStrength,
        kPORT_MuxAlt2,
        kPORT_InputBufferEnable,
        kPORT_InputNormal,
        kPORT_UnlockRegister
    };

    PORT_SetPinConfig(PORT1, 0U, &spi_pin_config); /* P1_0  LPSPI0_SDO  */
    PORT_SetPinConfig(PORT1, 1U, &spi_pin_config); /* P1_1  LPSPI0_SCK  */
    //PORT_SetPinConfig(PORT1, 3U, &spi_pin_config); /* P1_3  LPSPI0_PCS0 */

    //PORT_SetPinConfig(PORT1, 2U, &spi_pin_config); /* P1_2  LPSPI0_SDI 用于SPI通信回环测试 */
}

void BSP_SPI_Init(void)
{
    lpspi_master_config_t master_config;
    uint32_t src_clock_hz;

    /* LPSPI0时钟源：使用FRO12M，与工程其它模块保持一致 */
    CLOCK_SetClockDiv(kCLOCK_DivLPSPI0, 1U);
    CLOCK_AttachClk(kFRO12M_to_LPSPI0);

    CLOCK_EnableClock(kCLOCK_GateLPSPI0);
    RESET_ReleasePeripheralReset(kLPSPI0_RST_SHIFT_RSTn);

    bsp_spi_pin_init();

    LPSPI_MasterGetDefaultConfig(&master_config);
    master_config.baudRate                     = BSP_SPI_MASTER_BAUDRATE;
    master_config.bitsPerFrame                 = 8U;
    master_config.cpol                         = kLPSPI_ClockPolarityActiveHigh;
    master_config.cpha                         = kLPSPI_ClockPhaseFirstEdge;
    master_config.direction                    = kLPSPI_MsbFirst;
    master_config.whichPcs                     = BSP_SPI_MASTER_PCS_FOR_INIT;
    master_config.pcsToSckDelayInNanoSec        = 1000000000U / (master_config.baudRate * 2U);
    master_config.lastSckToPcsDelayInNanoSec    = 1000000000U / (master_config.baudRate * 2U);
    master_config.betweenTransferDelayInNanoSec = 1000000000U / (master_config.baudRate * 2U);

    src_clock_hz = CLOCK_GetLpspiClkFreq(0U);

    LPSPI_MasterInit(BSP_SPI_MASTER_BASEADDR, &master_config, src_clock_hz);
}

status_t BSP_SPI_WriteByte(uint8_t data)
{
    return BSP_SPI_WriteBuffer(&data, 1U);
}

// status_t BSP_SPI_WriteBuffer(const uint8_t *buffer, size_t length)
// {
//     lpspi_transfer_t transfer;

//     transfer.txData      = buffer;
//     transfer.rxData      = NULL;
//     transfer.dataSize    = length;
//     transfer.configFlags = BSP_SPI_MASTER_PCS_FOR_TRANSFER | kLPSPI_MasterByteSwap;

//     return LPSPI_MasterTransferBlocking(BSP_SPI_MASTER_BASEADDR, &transfer);
// }
status_t BSP_SPI_WriteBuffer(const uint8_t *buffer, size_t length){
    lpspi_transfer_t transfer;
    status_t status;
    uint32_t srAfterReturn;
    uint32_t fsrAfterReturn;
    bsp_spi_diag_sample_t sample;

    transfer.txData      = buffer;
    transfer.rxData      = NULL;
    transfer.dataSize    = length;
    transfer.configFlags = BSP_SPI_MASTER_PCS_FOR_TRANSFER | kLPSPI_MasterByteSwap;
    status = LPSPI_MasterTransferBlocking(BSP_SPI_MASTER_BASEADDR,&transfer);
    /*
     * SDK 返回后，首先读取硬件寄存器。
     *
     * 此处不要插入 PRINTF、延时、断点或等待空闲操作。
     * 直接读取寄存器，减少调试构建下函数调用带来的延迟。
     *
     * SR/FSR 的读取不清除状态标志。
     */
    srAfterReturn  = BSP_SPI_MASTER_BASEADDR->SR;
    fsrAfterReturn = BSP_SPI_MASTER_BASEADDR->FSR;

    /* 捕获之后再进行解析和 RAM 记录。 */
    sample.callIndex = s_spiDiagCalls + 1U;
    sample.length = (uint32_t)length;
    sample.status = status;
    sample.sr = srAfterReturn;
    sample.fsr = fsrAfterReturn;

    sample.txCount =
        (fsrAfterReturn & LPSPI_FSR_TXCOUNT_MASK) >> LPSPI_FSR_TXCOUNT_SHIFT;

    sample.mbf =
        ((srAfterReturn & (uint32_t)kLPSPI_ModuleBusyFlag) != 0U) ? 1U : 0U;

    sample.tcf =
        ((srAfterReturn & (uint32_t)kLPSPI_TransferCompleteFlag) != 0U)? 1U : 0U;

    s_spiDiagLast = sample;
    s_spiDiagCalls = sample.callIndex;

    if (status != kStatus_Success){
        ++s_spiDiagErrors;
    }
    else if ((sample.txCount != 0U) || (sample.mbf != 0U)){
        if (s_spiDiagSuccessPending == 0U){
            s_spiDiagFirstPending = sample;
        }
        /* 快照写完后再更新计数。 */
        ++s_spiDiagSuccessPending;
    }
    /* 保持原来的返回值和错误处理行为。 */
    return status;    
}

//临时测试函数
status_t BSP_SPI_LoopbackTest(void)
{
    uint8_t tx[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t rx[4] = {0};
    lpspi_transfer_t transfer = {
        .txData = tx,
        .rxData = rx,
        .dataSize = sizeof(tx),
        .configFlags = BSP_SPI_MASTER_PCS_FOR_TRANSFER
    };
    status_t st = LPSPI_MasterTransferBlocking(BSP_SPI_MASTER_BASEADDR, &transfer);
    if(st != kStatus_Success){
        PRINTF("SPI transfer failed, status=%d\r\n",(int)st);
        return st;
    }
    if (memcmp(tx, rx, 4U) != 0)
    {
        PRINTF("SPI loopback data mismatch.\r\n");
        PRINTF("TX: %02X %02X %02X %02X\r\n", tx[0], tx[1], tx[2], tx[3]);
        PRINTF("RX: %02X %02X %02X %02X\r\n", rx[0], rx[1], rx[2], rx[3]);
        return kStatus_Fail;
    }
    PRINTF("SPI loopback OK\r\n");
    return kStatus_Success;
}
