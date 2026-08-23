#include "app/ui_display_port.h"
#include "bsp/bsp_oled_12864.h"

#define UI_OLED_PANEL 1U    //使用1号屏幕

void UI_Display_Init(void){
    //该函数会拉动共享RST引脚，因此只能在系统启动时执行一次
    BSP_OLED_Init(UI_OLED_PANEL);
    //如果其他三块屏幕也要用，初始化调用
    //BSP_OLED_InitAll();
}

void UI_Display_Clear(void){
    BSP_OLED_Clear();
}

void UI_Display_Present(void){
    /*
     * 每次刷新前重新选择1号屏幕。
     * 即使其他代码改变过译码器地址，
     * UI内容也只会发送到1号屏幕。
     */
    BSP_OLED_SelectPanel(UI_OLED_PANEL);
    BSP_OLED_Refresh();
}
