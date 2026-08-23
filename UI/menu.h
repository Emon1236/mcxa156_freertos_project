//
#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// 按键功能枚举
typedef enum{
    MENU_KEY_UP = 0U,
    MENU_KEY_DOWN,
    MENU_KEY_OK,
    MENU_KEY_BACK
}menu_key_t;

void Menu_Init(void);//初始化菜单数据；新增初始化菜单动画位置

bool Menu_HandleKey(menu_key_t key);//处理按键事件；新增改变动画目标值，光标不再立刻跳转到最终位置

bool Menu_Update(uint32_t deltaTimeMs); //新增，更新一帧动画，返回true表示动画位置发生变化，当前帧需要重绘

void Menu_Render(void); //绘制菜单；新增将当前动画帧绘制到OLED帧缓冲，不再内部发送SPI数据

#ifdef __cplusplus
}
#endif

#endif /* APP_MENU_H */
