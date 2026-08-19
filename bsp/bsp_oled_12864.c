/*
SSD1315 OLED12864 x4 集成屏幕驱动，SPI总线共享，通过A0/A1和译码器选屏
RES/D-C复用原SDI/CS引脚；A0/A1为新增GPIO

*----------------------连线说明---------------------
VCC-3.3V                        RES-SDI(P1_2)
GND-GND                         D/C-CS(P1_3)
D0(SCK)-SCK(P1_1)               A0-P1_13
D1(MOSI)-SDO(P1_0)              A1-P1_12
*---------------------------------------------------
*----------------------片段地址映射表----------------
A0:A1           译码器Y0~Y3         硬件对应
0:0             0111                0号屏(左上角) 
0:1             1011                1号屏(右上角)
1:0             1101                2号屏(左下角)
1:1             1110                3号屏(右下角)
*---------------------------------------------------
*----------------------数据存储格式-------------------
 * 纵向8点，高位在下，先从左到右，再从上到下
 **      B0                  B0 B0
  *      B1                  B1 B1
  *      B2       0-127      B2 B2
  *      B3  ------------->  B3 B3 --
  *      B4                  B4 B4  |
  *      B5                  B5 B5  |
  *      B6                  B6 B6  |
  *      B7                  B7 B7  |
  *                                 |
  *  --------------------------------
  *  |   
  *  |   B0                  B0 B0
  *  |   B1                  B1 B1
  *  |   B2                  B2 B2
  *  --> B3  ------------->  B3 B3
  *      B4                  B4 B4
  *      B5                  B5 B5
  *      B6                  B6 B6
  *      B7                  B7 B7
  * 以ASCII字模为例，6x8点阵，宽6高8，纵向8行，每列1字节，共6列。
测试说明：0号屏幕顶层1/3区域存在坏点，按当前硬件分布，镜像后为底部1/3区域失能
*/
#include "bsp/bsp_oled_12864.h"
#include "bsp/bsp_oled_data.h"
#include "bsp/bsp_spi.h"
#include "board.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_reset.h"
#include <string.h>
#include <assert.h> //用于断言字体传入的合法性

/* RES: 原SDI(P1_2)，D/C: 原CS/PCS0(P1_3) */
#define OLED_RES_GPIO GPIO1
#define OLED_RES_PORT PORT1
#define OLED_RES_PIN  2U
#define OLED_DC_GPIO  GPIO1
#define OLED_DC_PORT  PORT1
#define OLED_DC_PIN   3U

/* A0: -> P1_13, A1: -> P1_12 */
#define OLED_A0_GPIO  GPIO1
#define OLED_A0_PORT  PORT1
#define OLED_A0_PIN   13U
#define OLED_A1_GPIO  GPIO1
#define OLED_A1_PORT  PORT1
#define OLED_A1_PIN   12U

/* 先只测试单屏，帧缓冲复用；后续扩展4屏可改成 s_oledBuffer[OLED_PANEL_COUNT][...] */
static uint8_t s_oledBuffer[OLED_HEIGHT / 8U][OLED_WIDTH];
/***********************************源文件内部函数**********************************************/
static void oled_gpio_pin_init(void)
{
    CLOCK_EnableClock(kCLOCK_GateGPIO1);//使能GPIO时钟
    CLOCK_EnableClock(kCLOCK_GateGPIO2);
    CLOCK_EnableClock(kCLOCK_GatePORT1);//使能PORT时钟
    CLOCK_EnableClock(kCLOCK_GatePORT2);

    RESET_ReleasePeripheralReset(kGPIO1_RST_SHIFT_RSTn);//释放GPIO复位
    RESET_ReleasePeripheralReset(kGPIO2_RST_SHIFT_RSTn);
    RESET_ReleasePeripheralReset(kPORT1_RST_SHIFT_RSTn);//释放PORT复位
    RESET_ReleasePeripheralReset(kPORT2_RST_SHIFT_RSTn);

    const port_pin_config_t gpio_pin_config = {
        kPORT_PullUp, kPORT_LowPullResistor, kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable,
        kPORT_LowDriveStrength, kPORT_NormalDriveStrength,
        kPORT_MuxAsGpio, kPORT_InputBufferEnable, kPORT_InputNormal,
        kPORT_UnlockRegister
    };
    PORT_SetPinConfig(OLED_RES_PORT, OLED_RES_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_DC_PORT, OLED_DC_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_A0_PORT, OLED_A0_PIN, &gpio_pin_config);
    PORT_SetPinConfig(OLED_A1_PORT, OLED_A1_PIN, &gpio_pin_config);

    gpio_pin_config_t out_config = {kGPIO_DigitalOutput, 1U};
    GPIO_PinInit(OLED_RES_GPIO, OLED_RES_PIN, &out_config);
    GPIO_PinInit(OLED_DC_GPIO, OLED_DC_PIN, &out_config);
    GPIO_PinInit(OLED_A0_GPIO, OLED_A0_PIN, &out_config);
    GPIO_PinInit(OLED_A1_GPIO, OLED_A1_PIN, &out_config);
}

static void oled_write_command(uint8_t cmd)
{
    GPIO_PinWrite(OLED_DC_GPIO, OLED_DC_PIN, 0U);
    BSP_SPI_WriteByte(cmd);
}
//根据物理安装位置针对性发送初始化指令
static void oled_controller_init(uint8_t panelIndex){
    if (panelIndex >= OLED_PANEL_COUNT){
        return;
    }
    BSP_OLED_SelectPanel(panelIndex);

    oled_write_command(0xAE);
    oled_write_command(0xD5);oled_write_command(0x80);
    oled_write_command(0xA8);oled_write_command(0x3F);
    oled_write_command(0xD3);oled_write_command(0x00);
    oled_write_command(0x40);
    oled_write_command(0x8D);oled_write_command(0x14);

    //0,1屏幕的物理安装方向相反，需要初始化时左右和上下翻转
    if (panelIndex == 0U || panelIndex == 1U){
        oled_write_command(0xA0);
        oled_write_command(0xC0);
    }else{
        oled_write_command(0xA1);
        oled_write_command(0xC8);
    }

    oled_write_command(0xDA);oled_write_command(0x12);    
    oled_write_command(0x81);oled_write_command(0xCF);
    oled_write_command(0xD9);oled_write_command(0xF1);
    oled_write_command(0xDB);oled_write_command(0x40);
    oled_write_command(0xA4);
    oled_write_command(0xA6);
    oled_write_command(0xAF);
}

static void oled_reset(void)
{
    /* RES为4屏共享线，复位时会同时复位所有屏 */
    GPIO_PinWrite(OLED_RES_GPIO, OLED_RES_PIN, 0U);
    for (volatile uint32_t i = 0U; i < 100000U; i++) {}
    GPIO_PinWrite(OLED_RES_GPIO, OLED_RES_PIN, 1U);
    for (volatile uint32_t i = 0U; i < 100000U; i++) {}
}

void BSP_OLED_SelectPanel(uint8_t index)
{
    /* 译码表: A0:A1=00->Y0 01->Y1 10->Y2 11->Y3，index即(A0<<1)|A1 */
    GPIO_PinWrite(OLED_A0_GPIO, OLED_A0_PIN, (index >> 1U) & 0x01U);
    GPIO_PinWrite(OLED_A1_GPIO, OLED_A1_PIN, index & 0x01U);
}

// void BSP_OLED_Init(uint8_t index)
// {
//     oled_gpio_pin_init();
//     oled_reset();
//     BSP_OLED_SelectPanel(index);
//     //OLED初始化指令
//     oled_write_command(0xAE);                           //设置显示开关，AE关闭，AF开启
//     oled_write_command(0xD5); oled_write_command(0x80); //设置时钟分频因子，震荡频率，00~FF
//     oled_write_command(0xA8); oled_write_command(0x3F); //设置多路复用比率，00~3F
//     oled_write_command(0xD3); oled_write_command(0x00); //设置显示偏移，00~7F
//     oled_write_command(0x40);                           //设置显示开始行，40~7F
//     oled_write_command(0x8D); oled_write_command(0x14); //设置电荷泵，14开启，10关闭
//     //oled_write_command(0x20); oled_write_command(0x00); //内存地址模式？
//     oled_write_command(0xA1);                           //设置左右方向，A1正常，A0左右翻转
//     oled_write_command(0xC8);                           //设置上下方向，C8正常，C0上下翻转
//     oled_write_command(0xDA); oled_write_command(0x12); //设置COM引脚硬件配置，12正常，02反向
//     oled_write_command(0x81); oled_write_command(0xCF); //设置对比度，00~FF
//     oled_write_command(0xD9); oled_write_command(0xF1); //设置预充电周期，F1正常，22~F1
//     oled_write_command(0xDB); oled_write_command(0x40); //设置VCOMH取消选择级别，
//     oled_write_command(0xA4);                           //设置全局显示开启，A4正常，A5全点亮
//     oled_write_command(0xA6);                           //设置正常/反色显示，A6正常，A7反相
//     oled_write_command(0xAF);                           //设置显示开关，AE关闭，AF开启

//     BSP_OLED_Clear();
//     BSP_OLED_Refresh();
// }
void BSP_OLED_Init(uint8_t index){
    if (index >= OLED_PANEL_COUNT){
        return;
    }

    oled_gpio_pin_init();   //引脚初始化
    oled_reset();           //复位屏幕
    oled_controller_init(index);//根据物理安装位置初始化屏幕控制器

    BSP_OLED_Clear();       //清空缓冲区
    BSP_OLED_SelectPanel(index);//更新译码电路，选择屏幕
    BSP_OLED_Refresh();     //刷新显示，将缓冲区数据写入屏幕
}

void BSP_OLED_Clear(void)
{
    memset(s_oledBuffer, 0, sizeof(s_oledBuffer));
}

void BSP_OLED_Refresh(void)
{
    for (uint8_t page = 0U; page < (OLED_HEIGHT / 8U); page++)
    {
        oled_write_command(0xB0U + page);
        oled_write_command(0x00);
        oled_write_command(0x10);
        GPIO_PinWrite(OLED_DC_GPIO, OLED_DC_PIN, 1U);
        BSP_SPI_WriteBuffer(s_oledBuffer[page], OLED_WIDTH);
    }
}

void BSP_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t fill)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
    {
        return;
    }
    uint8_t page = y / 8U;
    uint8_t bit  = y % 8U;
    if (fill)
    {
        s_oledBuffer[page][x] |= (uint8_t)(1U << bit);
    }
    else
    {
        s_oledBuffer[page][x] &= (uint8_t)~(1U << bit);
    }
}

void BSP_OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    int16_t dx  = (int16_t)x1 - (int16_t)x0;
    int16_t dy  = (int16_t)y1 - (int16_t)y0;
    int16_t sx  = (dx > 0) ? 1 : -1;
    int16_t sy  = (dy > 0) ? 1 : -1;
    int16_t adx = (dx > 0) ? dx : -dx;
    int16_t ady = (dy > 0) ? dy : -dy;
    int16_t err = adx - ady;
    int16_t x   = (int16_t)x0;
    int16_t y   = (int16_t)y0;

    while (1)
    {
        BSP_OLED_DrawPoint((uint8_t)x, (uint8_t)y, 1U);
        if (x == x1 && y == y1)
        {
            break;
        }
        int16_t e2 = err * 2;
        if (e2 > -ady) { err -= ady; x += sx; }
        if (e2 < adx)  { err += adx; y += sy; }
    }
}


/**
 * 函    数：OLED显示图像
 * 参    数：X 指定图像左上角的横坐标，范围：0~127
 * 参    数：Y 指定图像左上角的纵坐标，范围：0~63
 * 参    数：Width 指定图像的宽度，范围：0~128
 * 参    数：Height 指定图像的高度，范围：0~64
 * 参    数：支持任意高度，不要求必须是8的整数倍
 * 返 回 值：无
 * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
 */
void BSP_OLED_ShowImage(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *Image){
    uint8_t pageCount;

    assert(Image != NULL);
    assert(width > 0U && width <= OLED_WIDTH);
    assert(height > 0U && height <= OLED_HEIGHT);

    if ((Image == NULL) || (width == 0U) || (height == 0U)){
        return;
    }

    pageCount = (uint8_t)((height + 7u) / 8u); //计算需要的页数，向上取整

    for(uint8_t page = 0U; page < pageCount; page++){
        for(uint8_t col = 0U; col < width; col++){
            uint16_t dataIndex = (uint16_t)page * width + col;
            uint8_t imageData = Image[dataIndex];
            for(uint8_t bit =0U; bit < 8U; bit++){
                uint16_t imageX = (uint16_t)x + col;
                uint16_t imageY = (uint16_t)y + page * 8U + bit;
                //最后一页可能不足8行，例如高度为10时，第二页只有2行有效数据
                if(((uint16_t)page * 8U + bit) >= height){
                    break; //跳出当前页的循环，避免越界
                }
                // 支持图片在屏幕边缘被裁剪，不会访问帧缓冲区之外的区域。
                if((imageX >= OLED_WIDTH) || (imageY >= OLED_HEIGHT)){
                    continue; //跳过超出屏幕范围的像素
                }
                BSP_OLED_DrawPoint((uint8_t)imageX, (uint8_t)imageY, (imageData >> bit) & 0x01U);
            }
        }
    }
}

void BSP_OLED_ShowChar(uint8_t x, uint8_t y, char ch, oled_font_t font){
    const uint8_t *glyph;
    uint8_t glyphIndex;

    assert(font == OLED_FONT_6X8 || font == OLED_FONT_8X16);//断言字体传入的合法性
    //避免正式版依旧传入不合法字体类型
    if((font != OLED_FONT_6X8) && (font != OLED_FONT_8X16)){
        return; //字体不合法，直接返回
    }
    if((ch < OLED_ASCII_FIRST_CHAR) ||(ch > OLED_ASCII_LAST_CHAR)){
        ch = '?';
    } // 如果传入空格到"~"之外的字符，则显示问号
    glyphIndex = (uint8_t)(ch - OLED_ASCII_FIRST_CHAR);
    if(font == OLED_FONT_6X8){
        glyph = &OLED_F6X8[(uint16_t)glyphIndex * 6U ];//每个6x8字符占6字节
        BSP_OLED_ShowImage(x, y, 6U, 8U, glyph);
    }else{
        glyph = &OLED_F8X16[(uint16_t)glyphIndex * 16U ];//每个8x16字符占16字节
        BSP_OLED_ShowImage(x, y, 8U, 16U, glyph);
    }
}

void BSP_OLED_ShowString(uint8_t x, uint8_t y, const char *str, oled_font_t font){
    uint8_t charWidth;
    uint8_t charHeight;
    uint8_t cursorX = x;
    uint8_t cursorY = y;

    assert(str != NULL);
    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));

    if (str == NULL){
        return;
    }

    if (font == OLED_FONT_6X8){
        charWidth  = 6U;
        charHeight = 8U;
    }
    else if (font == OLED_FONT_8X16){
        charWidth  = 8U;
        charHeight = 16U;
    }
    else{
        return;
    }
    while(*str != '\0'){
        if(*str == '\n'){
            cursorX = x;
            cursorY = (uint8_t)(cursorY + charHeight);
            str++;
            continue;
        }
        if((cursorX + charWidth) > OLED_WIDTH){ //当前行放不下则换行
            cursorX = x;
            cursorY = (uint8_t)(cursorY + charHeight);
        }
        if((uint16_t)cursorY + charHeight > OLED_HEIGHT){ //超出屏幕高度则停止显示
            break;
        }
        BSP_OLED_ShowChar(cursorX, cursorY, *str, font);
        cursorX = (uint8_t)(cursorX + charWidth);
        str++;
    }
}

void BSP_OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num, oled_font_t font){
    char digits[10];
    uint8_t length = 0U;
    uint8_t charWidth;  

    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));

    if (font == OLED_FONT_8X16){
        charWidth  = 8U;
    }
    else if (font == OLED_FONT_6X8){
        charWidth  = 6U;
    }
    else{
        return;
    }
    if (num == 0U){
        BSP_OLED_ShowChar(x, y, '0', font);
        return;
    }
    while (num > 0U && length < sizeof(digits)){
        digits[length++] = (char)('0' + (num % 10U));
        num /= 10U;
    }
    for (uint8_t i = 0U; i < length; i++){
        uint16_t drawX = (uint16_t)x + (uint16_t)i *charWidth;
        if (drawX + charWidth > OLED_WIDTH){
            break; //超出屏幕宽度则停止显示
        }
        BSP_OLED_ShowChar((uint8_t)drawX, y, digits[length - 1U - i], font);
    }
}


/*********************************************************************************/
//新增四屏初始化
void BSP_OLED_InitAll(void){
    oled_gpio_pin_init();   //引脚初始化
    oled_reset();           //四块屏幕共享RES复位，统一复位一次

    for(uint8_t panel=0U; panel < OLED_PANEL_COUNT; panel++){
        oled_controller_init(panel);//译码电路无法全选，因此依次选择和初始化屏幕
    }
    BSP_OLED_Clear();       //清空缓冲区
    for (uint8_t panel = 0U;panel < OLED_PANEL_COUNT; panel++){
        BSP_OLED_SelectPanel(panel);//更新译码电路，选择屏幕
        BSP_OLED_Refresh();     //刷新显示，将缓冲区数据写入屏幕
    }
}



