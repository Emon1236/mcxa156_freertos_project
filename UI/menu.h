
#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//最多允许的菜单层数
#define MENU_MAX_DEPTH 4U
//菜单左侧留出空白
#define MENU_TEXT_X 4U
//文字上下各留一个像素
#define MENU_LINE_PADDING 2U

//按键音参数
#define MENU_KEY_TONE_HZ 2000U
#define MENU_KEY_TONE_MS 50U

//动画响应时间，数值越小动画越快
#define MENU_SCROLL_RESPONSE_MS 90U //滚动响应
#define MENU_CURSOR_RESPONSE_MS 80U //光标响应
#define MENU_CURSOR_WIDTH_RESPONSE_MS 100U  //光标宽度响应

/*
 * 一级菜单图标布局参数
 * 图标30x30像素，中心距离36像素，左侧标题指示器7x27像素，标题使用18像素粗体
 */
#define MENU_TILE_ICON_SPACING              36.0f
#define MENU_TILE_INDICATOR_X               0U
#define MENU_TILE_INDICATOR_Y               36U
#define MENU_TILE_INDICATOR_WIDTH           7.0f
#define MENU_TILE_INDICATOR_HEIGHT          27U
//标题最终使用基线坐标y=58，切换菜单时先放到屏幕下方y=76，再向上滑到y=58
#define MENU_TILE_TITLE_START_Y              76.0f
#define MENU_TILE_TITLE_TARGET_Y             58.0f
//动画沿用UI_Animation_Approach，按毫秒计算，取代源工程依赖主循环频率的动画函数
#define MENU_TILE_ICON_RESPONSE_MS           110U
#define MENU_TILE_ENTRY_RESPONSE_MS          100U
#define MENU_TILE_TITLE_RESPONSE_MS          90U
#define MENU_TILE_INDICATOR_RESPONSE_MS      90U

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
