/*
显示接口头文件
规定UI主界面显示在1号屏幕上(0,1,2,3)
*/
#ifndef UI_DISPLAY_PORT_H
#define UI_DISPLAY_PORT_H



#ifdef __cplusplus
extern "C" {
#endif
//UI显示屏幕分辨率
#define UI_DISPLAY_WIDTH 128U
#define UI_DISPLAY_HEIGHT 64U

void UI_Display_Init(void);
//帧缓冲函数
void UI_Display_Clear(void);
void UI_Display_Present(void);

#ifdef __cplusplus
}
#endif

#endif