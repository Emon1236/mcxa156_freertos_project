#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif
// 弹窗输入事件，menu.c把MENU_KEY转换成此处的事件
typedef enum{
    UI_WINDOW_INPUT_DECREASE = 0U,  //减少
    UI_WINDOW_INPUT_INCREASE,       //增加
    UI_WINDOW_INPUT_CONFIRM,        //确认
    UI_WINDOW_INPUT_CANCEL          //取消
} ui_window_input_t;

// 数值变化回调
typedef void (*ui_window_value_callback_t)(uint8_t value);

//打开一个数值调节弹窗
bool UI_Window_OpenValue(const char *title, uint8_t initialValue, uint8_t minimumValue,
                    uint8_t maximumValue, uint8_t step, ui_window_value_callback_t valueChanged);

//返回当前弹窗是否存在，包括进入和退出动画阶段
bool UI_Window_IsVisible(void);
//处理一次弹窗输入
bool UI_Window_HandleInput(ui_window_input_t input);
//更新弹窗动画状态，返回true表示动画位置发生变化
bool UI_Window_Update(uint32_t deltaTimeMs);
//在当前u8g2缓冲区上叠加绘制弹窗，该函数不会清空缓冲区，也不调用sendbuffer
void UI_Window_Draw(u8g2_t *u8g2);
// 设置弹窗动画响应时间，单位为毫秒
void UI_Window_SetAnimationResponseMs(uint16_t responseMs);


#ifdef __cplusplus
}
#endif

#endif /* UI_WINDOW_H */