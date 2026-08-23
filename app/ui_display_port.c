#include "app/ui_display_port.h"

#include "bsp/bsp_u8g2_port.h"

void UI_Display_Init(void)
{
    BSP_U8G2_Init();
}

void UI_Display_Clear(void)
{
    u8g2_ClearBuffer(BSP_U8G2_GetContext());
}

void UI_Display_Present(void)
{
    /*
     * 选择1号屏幕的操作已经放在U8g2的
     * START_TRANSFER回调中。
     */
    u8g2_SendBuffer(BSP_U8G2_GetContext());
}

u8g2_t *UI_Display_GetContext(void)
{
    return BSP_U8G2_GetContext();
}