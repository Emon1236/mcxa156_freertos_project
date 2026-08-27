/*
当前链接脚本为MCU数据区分配了0x1E000，大约120KB
温度环形缓冲区256xint16_t = 512字节
波形帧缓冲为256x64/8 = 2048字节
合计2560字节，占用MCU数据区约2.1%，不建议用pvPortMalloc动态创建帧缓冲；静态2KB更确定
也不会产生堆碎片。
推出动画直接改变绘图的Y偏移，复用同一个2048字节缓冲，不保存第二幅动画画面。
*/

#include "bsp/bsp_2oleds.h"
#include "bsp/bsp_oled_hw.h"
#include "bsp/bsp_spi.h"

#include <stddef.h>
#include <stdint.h>

//物理屏幕编号
#define BSP_2OLEDS_LEFT_PANEL    2U
#define BSP_2OLEDS_RIGHT_PANEL   3U
#define BSP_2OLEDS_PANEL_WIDTH       128U

static bool s_pairInitialized;

//全0页放在只读存储区，不占用RAM的128字节
static const uint8_t s_emptyPage[BSP_2OLEDS_PANEL_WIDTH] = {0U};
//向当前已经选中的OLED发送命令
static bool bsp_2oleds_write_command(const uint8_t *commands, size_t commandCount){
    if ((commands == NULL) || (commandCount == 0U)) {
        return false;
    }
    BSP_OLED_HW_SetDataMode(0U);

    return BSP_OLED_HW_Write(commands, commandCount);
}

static bool bsp_2oleds_write_data(const uint8_t *data, size_t dataSize){
    if ((data == NULL) || (dataSize == 0U)) {
        return false;
    }
    BSP_OLED_HW_SetDataMode(1U);

    return BSP_OLED_HW_Write(data, dataSize);
}

//2,3号屏幕原点位于左上角，使用A1正常列扫描方向，C8正常COM扫描方向
static bool bsp_2oleds_init_controller(uint8_t panel){
    static const uint8_t initCommands[] = {
        0xAEU,             /* 关闭显示。 */

        0xD5U, 0x80U,      /* 显示时钟分频。 */
        0xA8U, 0x3FU,      /* 64路复用。 */
        0xD3U, 0x00U,      /* 显示偏移为0。 */
        0x40U,             /* 显示起始行为0。 */

        0x8DU, 0x14U,      /* 开启内部电荷泵。 */

        0x20U, 0x02U,      /* 页地址模式。 */

        0xA1U,             /* 2、3号屏左右方向正常。 */
        0xC8U,             /* 2、3号屏上下方向正常。 */

        0xDAU, 0x12U,      /* COM引脚配置。 */
        0x81U, 0xCFU,      /* 对比度。 */
        0xD9U, 0xF1U,      /* 预充电周期。 */
        0xDBU, 0x40U,      /* VCOMH。 */

        0xA4U,             /* 使用显示RAM内容。 */
        0xA6U,             /* 正常显示，不反色。 */
        0xAFU              /* 开启显示。 */
    };

    BSP_OLED_HW_SelectPanel(panel);

    return bsp_2oleds_write_command(initCommands, sizeof(initCommands));
}
/**
 * @brief 发送帧缓冲到指定的OLED面板
 * 
 * @param panel
 * @param *frameBuffer
 * @param sourceStride 原始帧缓冲每一页的宽度，256
 * @param sourceX 2号屏幕取0，3号屏幕取128
 * @return true 
 * @return false 
 */
static bool bsp_2oleds_send_panel(uint8_t panel, const uint8_t *frameBuffer, uint16_t sourceStride, uint16_t sourceX){
    uint8_t page;
    BSP_OLED_HW_SelectPanel(panel);
    for (page = 0U; page < BSP_2OLEDS_PAGE_COUNT; page++){
        uint8_t pageCommands[3];
        const uint8_t *source;

        pageCommands[0] = (uint8_t)(0xB0U | page);
        pageCommands[1] = 0x00U;  /* 设置列地址低位为0。 */
        pageCommands[2] = 0x10U;  /* 设置列地址高位为0。 */

        if (!bsp_2oleds_write_command(pageCommands, sizeof(pageCommands))) {
            return false;
        }
        source = &frameBuffer[(uint16_t)page * sourceStride + sourceX];
        //直接从256缓冲中连续发送128字节
        if(!bsp_2oleds_write_data(source, BSP_2OLEDS_PANEL_WIDTH)){
            return false;
        }
    }
    return true;
}

static bool bsp_2oleds_clear_panel(uint8_t panel){
    uint8_t page;
    BSP_OLED_HW_SelectPanel(panel);
    for (page = 0U; page < BSP_2OLEDS_PAGE_COUNT; page++){
        uint8_t pageCommands[3];

        pageCommands[0] = (uint8_t)(0xB0U + page);
        pageCommands[1] = 0x00U;
        pageCommands[2] = 0x10U;        
        if(!bsp_2oleds_write_command(pageCommands, sizeof(pageCommands))){
            return false;
        }
        if(!bsp_2oleds_write_data(s_emptyPage, sizeof(s_emptyPage))){
            return false;
        }
    }
    return true;
}
/**
 * @brief 初始化2号和3号OLED面板
 * 
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool BSP_2OLEDS_Init(void){
    bool success;
    if (s_pairInitialized){
        return true;
    }
    //app_ui_task保证本函数在UI_Display_Init()之后调用
    /*
    此时SPI初始化，OLED GPIO初始化，共享RESET由U8G2执行，1号屏幕完成初始化
    */
    BSP_OLED_HW_Init();
    success = bsp_2oleds_init_controller(BSP_2OLEDS_LEFT_PANEL);
    if (success){
        success = bsp_2oleds_init_controller(BSP_2OLEDS_RIGHT_PANEL);
    }
    //右侧屏幕也初始化成功后，执行清屏
    if (success){
        success = BSP_2OLEDS_Clear();
    }
    //初始化完成后恢复选择1号屏幕，便于后续U8G2刷新
    BSP_OLED_HW_SelectPanel(BSP_OLED_UI_PANEL);
    BSP_OLED_HW_SetDataMode(1U);
    if(success){
        s_pairInitialized = true;
    }
    return success;
}
/**
 * @brief 将帧缓冲显示到2号和3号OLED面板
 * 
 * @param frameBuffer 帧缓冲，大小为BSP_OLED_PAIR23_FRAME_BYTES
 * @return true 显示成功
 * @return false 显示失败
 */
bool BSP_2OLEDS_Present(const uint8_t frameBuffer[BSP_2OLEDS_FRAME_BYTES]){
    bool success;
    success = bsp_2oleds_send_panel(BSP_2OLEDS_LEFT_PANEL, frameBuffer, BSP_2OLEDS_WIDTH, 0U);
    if(success){
        success = bsp_2oleds_send_panel(BSP_2OLEDS_RIGHT_PANEL, frameBuffer, BSP_2OLEDS_WIDTH, BSP_2OLEDS_PANEL_WIDTH);
    }
    //多屏刷新后需要恢复1号屏幕的选择
    BSP_OLED_HW_SelectPanel(BSP_OLED_UI_PANEL);
    BSP_OLED_HW_SetDataMode(1U);
    return success;
}
/**
 * @brief 清除2号和3号OLED面板
 * 
 * @return true 清除成功
 * @return false 清除失败
 */
bool BSP_2OLEDS_Clear(void){
    bool success;
    success = bsp_2oleds_clear_panel(BSP_2OLEDS_LEFT_PANEL);
    if(success){
        success = bsp_2oleds_clear_panel(BSP_2OLEDS_RIGHT_PANEL);
    }
    //清屏后恢复1号屏幕的选择
    BSP_OLED_HW_SelectPanel(BSP_OLED_UI_PANEL);
    BSP_OLED_HW_SetDataMode(1U);
    return success;
}
