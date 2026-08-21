#include "bsp/bsp_4oleds.h"
#include "bsp/bsp_oled_data.h"

#include <assert.h>
#include <string.h>

// 256x128虚拟帧缓冲区，数据格式：s_oled4Buffer[page][x]，
// page0表示y=0~7，page15表示y=120~127。占用RAM = 4096字节
static uint8_t s_oled4Buffer[OLED_PAGE_COUNT][OLED4_WIDTH];
//每块屏幕占用128x64/8=1024字节，暂存某一块OLED数据
static uint8_t s_panelImage[OLED4_PANEL_HEIGHT / 8U][OLED4_PANEL_WIDTH];

void BSP_4OLEDs_Init(void){
    /*
     * 初始化共享SPI、GPIO和四块OLED控制器。
     * BSP_SPI_Init()应在系统其他地方初始化一次。
     */
    BSP_OLED_InitAll();

    BSP_4OLEDs_Clear();
    BSP_4OLEDs_Refresh();
}

void BSP_4OLEDs_Clear(void){
    memset(s_oled4Buffer, 0, sizeof(s_oled4Buffer));
}

void BSP_4OLEDs_DrawPoint(uint16_t x, uint16_t y, uint8_t fill){
    uint16_t page;
    uint8_t bit;

    if ((x >= OLED4_WIDTH) || (y >= OLED4_HEIGHT)){
        return;
    }

    page = y / 8U;
    bit  = (uint8_t)(y % 8U);

    if (fill != 0U){
        s_oled4Buffer[page][x] |=
            (uint8_t)(1U << bit);
    }
    else{
        s_oled4Buffer[page][x] &=
            (uint8_t)~(1U << bit);
    }
}

void BSP_4OLEDs_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
    int32_t x = (int32_t)x0;
    int32_t y = (int32_t)y0;

    int32_t endX = (int32_t)x1;
    int32_t endY = (int32_t)y1;

    int32_t dx = (endX >= x) ? (endX - x) : (x - endX);
    int32_t dy = (endY >= y) ? (endY - y) : (y - endY);

    int32_t stepX = (x < endX) ? 1 : -1;
    int32_t stepY = (y < endY) ? 1 : -1;

    int32_t error = dx - dy;

    while (1){
        BSP_4OLEDs_DrawPoint((uint16_t)x,
                             (uint16_t)y,
                             1U);

        if ((x == endX) && (y == endY)){
            break;
        }

        int32_t doubleError = 2 * error;

        if (doubleError > -dy){
            error -= dy;
            x += stepX;
        }

        if (doubleError < dx){
            error += dx;
            y += stepY;
        }
    }
}

void BSP_4OLEDs_ShowImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image){
    uint16_t imagePageCount;

    assert(image != NULL);

    if ((image == NULL) || (width == 0U) || (height == 0U)){
        return;
    }

    imagePageCount = (height + 7U) / 8U;

    for (uint16_t page = 0U; page < imagePageCount; page++){
        for (uint16_t column = 0U; column < width; column++){
            uint32_t imageIndex = (uint32_t)page * width + column;

            uint8_t imageData = image[imageIndex];

            for (uint8_t bit = 0U; bit < 8U; bit++){
                uint16_t imagePixelY = (uint16_t)(page * 8U + bit);
                //当图片高度不是8的整数倍时，最后一个字节中的部分bit无效。
                if (imagePixelY >= height){
                    break;
                }
                BSP_4OLEDs_DrawPoint((uint16_t)(x + column), (uint16_t)(y + imagePixelY), (uint8_t)((imageData >> bit) & 0x01U));
            }
        }
    }
}

void BSP_4OLEDs_ShowChar(uint16_t x, uint16_t y, char ch, oled_font_t font){
    uint8_t glyphIndex;
    const uint8_t *glyph;

    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));
    if ((font != OLED_FONT_6X8) && (font != OLED_FONT_8X16)){
        return;
    }

    //字库只包含空格到~。超出范围的字符统一显示为问号。
    if ((ch < OLED_ASCII_FIRST_CHAR) || (ch > OLED_ASCII_LAST_CHAR)){
        ch = '?';
    }

    glyphIndex = (uint8_t)(ch - OLED_ASCII_FIRST_CHAR);

    if (font == OLED_FONT_6X8){
        glyph = &OLED_F6X8[(uint16_t)glyphIndex * 6U];
        BSP_4OLEDs_ShowImage(
            x, y, 6U, 8U,glyph);
    }
    else{
        glyph = &OLED_F8X16[(uint16_t)glyphIndex * 16U];

        BSP_4OLEDs_ShowImage(
            x, y, 8U, 16U, glyph);
    }
}

void BSP_4OLEDs_ShowString(uint16_t x, uint16_t y, const char *str, oled_font_t font){
    uint16_t cursorX = x;
    uint16_t cursorY = y;

    uint8_t charWidth;
    uint8_t charHeight;

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

    while (*str != '\0'){
        if (*str == '\n'){
            cursorX = x;
            cursorY += charHeight;
            str++;
            continue;
        }

        //当前行剩余空间不足时自动换行。
        if ((cursorX + charWidth) > OLED4_WIDTH){
            cursorX = x;
            cursorY += charHeight;
        }

        //虚拟屏幕剩余高度不足时停止。
        if ((cursorY + charHeight) > OLED4_HEIGHT){
            break;
        }

        BSP_4OLEDs_ShowChar(
            cursorX, cursorY, *str, font);
        cursorX += charWidth;
        str++;
    }
}

void BSP_4OLEDs_ShowNumber(uint16_t x, uint16_t y, uint32_t number, oled_font_t font){
    char digits[10];
    uint8_t length = 0U;
    uint8_t charWidth;

    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));

    if (font == OLED_FONT_6X8){
        charWidth = 6U;
    }
    else if (font == OLED_FONT_8X16){
        charWidth = 8U;
    }
    else{
        return;
    }

    if (number == 0U){
        BSP_4OLEDs_ShowChar(x, y, '0', font);
        return;
    }

    /*
     * 先按从低位到高位的顺序保存数字。
     */
    while ((number > 0U) && (length < sizeof(digits))){
        digits[length++] = (char)('0' + number % 10U);
        number /= 10U;
    }

    /*
     * 再反向读取，按正常顺序显示。
     */
    for (uint8_t i = 0U; i < length; i++){
        uint16_t drawX = (uint16_t)(x + (uint16_t)i * charWidth);
        if ((drawX + charWidth) > OLED4_WIDTH){
            break;
        }
        BSP_4OLEDs_ShowChar(drawX, y, digits[length - 1U - i], font);
    }
}

void BSP_4OLEDs_Refresh(void){
    /*
     * 物理屏幕排列：
     *
     * panel 0 | panel 1
     * --------+--------
     * panel 2 | panel 3
     */
    for (uint8_t panel = 0U; panel < OLED4_PANEL_COUNT; panel++){
        uint8_t panelColumn = panel % 2U;
        uint8_t panelRow    = panel / 2U;

        uint16_t sourceX = (uint16_t)panelColumn * OLED4_PANEL_WIDTH;
        uint16_t sourcePage = (uint16_t)panelRow * (OLED4_PANEL_HEIGHT / 8U);

        /*
         * 从256×128虚拟帧缓冲区中取出当前物理屏幕的区域。
         */
        for (uint8_t page = 0U; page < (OLED4_PANEL_HEIGHT / 8U); page++){
            memcpy(s_panelImage[page], &s_oled4Buffer[sourcePage + page][sourceX], OLED4_PANEL_WIDTH);
        }

        /*
         * 当前单屏驱动只有一份128×64帧缓冲区，
         * 因此每次把一块屏幕的数据装入该缓冲区，
         * 然后发送给对应物理屏幕。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowImage(0U, 0U, OLED4_PANEL_WIDTH, OLED4_PANEL_HEIGHT, &s_panelImage[0][0]);

        BSP_OLED_SelectPanel(panel);
        BSP_OLED_Refresh();
    }
}

void BSP_4OLEDs_FillRectangle(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t fill){
    uint16_t drawWidth;
    uint16_t drawHeight;

    if((width == 0u) || (height == 0U)){
        return;
    }
    if((x >= OLED4_WIDTH) || (y >= OLED4_HEIGHT)){
        return;
    }

    //对超出虚拟屏幕边界的部分进行裁剪
    drawWidth = width;
    drawHeight = height;
    if(drawWidth > (OLED4_WIDTH - x)){
        drawWidth = (uint16_t)(OLED4_WIDTH - x);
    }
    if(drawHeight > (OLED4_HEIGHT - y)){
        drawHeight = (uint16_t)(OLED4_HEIGHT - y);
    }
    for(uint16_t py = 0U; py < drawHeight; py++){
        for(uint16_t px = 0U;px < drawWidth; px++){
            BSP_4OLEDs_DrawPoint((uint16_t)(x + px),(uint16_t)(y + py),fill);
        }
    }
}

void BSP_4OLEDs_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height){
    uint16_t drawWidth;
    uint16_t drawHeight;
    uint16_t right;
    uint16_t bottom;

    if((width == 0u) || (height == 0U)){
        return;
    }
    if((x >= OLED4_WIDTH) || (y >= OLED4_HEIGHT)){
        return;
    } 
    //将矩形裁剪到虚拟屏幕范围内
    drawWidth = width;
    drawHeight = height;
    if(drawWidth > (OLED4_WIDTH - x)){
        drawWidth = (uint16_t)(OLED4_WIDTH - x);
    }
    if(drawHeight > (OLED4_HEIGHT - y)){
        drawHeight = (uint16_t)(OLED4_HEIGHT - y);
    }
    right = (uint16_t)(x + drawWidth -1U);
    bottom = (uint16_t)(y + drawHeight -1U);

    BSP_4OLEDs_DrawLine(x,y, right,y);
    BSP_4OLEDs_DrawLine(x,bottom, right,bottom);
    BSP_4OLEDs_DrawLine(x,y, x,bottom);
    BSP_4OLEDs_DrawLine(right,y, right,bottom);
}
//显示反色字符
/*
 * 不能简单地先填充白色背景，再调用普通ShowChar，
 * 因为普通ShowChar仍然会写入正常字模。
 * 这里先把字模逐字节取反，再调用ShowImage。
*/
void BSP_4OLEDs_ShowCharInverted(uint16_t x, uint16_t y, char ch, oled_font_t font){
    const uint8_t *sourceGlyph;
    uint8_t invertedGlyph[16];

    uint8_t charCode;
    uint8_t glyphIndex;
    uint8_t glyphWidth;
    uint8_t glyphHeight;
    uint8_t glyphSize;  
    
    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));
    if ((font != OLED_FONT_6X8) && (font != OLED_FONT_8X16)){
        return;
    }
    charCode = (uint8_t)ch;
    // 非法字符使用问号替代
    if((charCode < OLED_ASCII_FIRST_CHAR) || (charCode > OLED_ASCII_LAST_CHAR)){
        charCode = '?';
    }
    glyphIndex = (uint8_t)(charCode - OLED_ASCII_FIRST_CHAR);
    if(font == OLED_FONT_6X8){
        glyphWidth = 6U;
        glyphHeight = 8U;
        glyphSize = 6U;

        sourceGlyph = &OLED_F6X8[(uint16_t)glyphIndex * glyphSize];
    }
    else{
        glyphWidth = 8U;
        glyphHeight = 16U;
        glyphSize = 16U;

        sourceGlyph = &OLED_F8X16[(uint16_t)glyphIndex * glyphSize];
    }
    //将原始字模中的每个字节逐位取反
    for(uint8_t i = 0U; i < glyphSize; i++){
        invertedGlyph[i] = (uint8_t)~sourceGlyph[i];
    }
    BSP_4OLEDs_ShowImage(x, y, glyphWidth, glyphHeight, invertedGlyph);
}
//反色字符串
void BSP_4OLEDs_ShowStringInverted(uint16_t x, uint16_t y, const char *str, oled_font_t font){
    uint16_t cursorX = x;
    uint16_t cursorY = y;

    uint8_t charWidth;
    uint8_t charHeight; 
    
    assert((font == OLED_FONT_6X8) || (font == OLED_FONT_8X16));
    if ((font != OLED_FONT_6X8) && (font != OLED_FONT_8X16)){
        return;
    }
    if(str == NULL){
        return;
    }
    if(font == OLED_FONT_6X8){
        charWidth = 6U;
        charHeight = 8U;
    }
    else if(font == OLED_FONT_8X16){
        charWidth = 8U;
        charHeight = 16U;
    }else{
        return;
    }
    while(*str != '\0'){
        if(*str == '\n'){
            cursorX = x;
            cursorY = (uint16_t)(cursorY + charHeight);
            str++;
            continue;
        }
        //当前行剩余空间不足时自动换行。
        if((cursorX + charWidth) > OLED4_WIDTH){
            cursorX = x;
            cursorY = (uint16_t)(cursorY + charHeight);
        }
        //剩余高度不足，停止显示
        if((cursorY + charHeight) > OLED4_HEIGHT){
            break;
        }
        BSP_4OLEDs_ShowCharInverted(cursorX, cursorY, *str, font);
        cursorX = (uint16_t)(cursorX + charWidth);
        str++;
    }
}
