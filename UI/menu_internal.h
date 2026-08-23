/*
关于typedef struct和typedef enum的使用说明：
    typedef struct用于定义结构体类型，例如menu_item_t就是这个结构体类型的别名，
        结构体类型表示一组相关数据组成的对象
        menu_item_t item;才是定义的变量
    typedef enum用于定义枚举类型，例如menu_key_t就是这个枚举类型的别名，
        枚举类型表示一组相关的常量值，一个变量只能处于若干个离散状态之一
        typedef enum {...} menu_view_t;定义的是类型而不是变量
        static menu_view_t s_currentView;才是定义的变量
函数指针的typedef是什么
    typedef void (*menu_action_t)(void); //定义一个函数指针类型，表示指向无参数，无返回值函数的函数指针类型
*/
//将原本位于menu.c的结构体和枚举类型定义提取到这里。

#ifndef UI_MENU_INTERNAL_H
#define UI_MENU_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/*
 * menu_item和menu_page相互引用，
 * 因此先提前声明两个结构体类型。
 */
typedef struct menu_item menu_item_t;
typedef struct menu_page menu_page_t;

/* 菜单项动作回调：无参数、无返回值。 */
typedef void (*menu_action_t)(void);

/* 动态菜单文字获取回调：返回只读字符串。 */
typedef const char *(*menu_label_getter_t)(void);

/* 一个菜单项。 */
struct menu_item
{
    /* 固定显示文字；使用动态文字时可以设为NULL。 */
    const char *label;

    /* 动态文字获取函数；使用固定文字时可以设为NULL。 */
    menu_label_getter_t getLabel;

    /* 指向下一级菜单；没有子菜单时设为NULL。 */
    const menu_page_t *childPage;

    /* 菜单动作回调；进入子菜单时通常设为NULL。 */
    menu_action_t action;
};

/* 一个菜单页面。 */
struct menu_page
{
    /* 指向该页面的菜单项数组。 */
    const menu_item_t *items;

    /* 菜单项数量。 */
    uint8_t itemCount;
};

/* 当前一层菜单的导航状态。 */
typedef struct
{
    const menu_page_t *page;

    uint8_t selectedIndex;
    uint8_t firstVisibleIndex;
    uint8_t cursorRow;
} menu_frame_t;

/* 菜单当前显示模式。 */
typedef enum
{
    MENU_VIEW_LIST = 0U,
    MENU_VIEW_NOTICE
} menu_view_t;

// 列表动画状态
typedef enum{
    MENU_LIST_PHASE_ENTERING = 0U,
    MENU_LIST_PHASE_ACTIVE, //正常处理按键
    MENU_LIST_PHASE_EXITING //列表收拢，完成后返回上一层
} menu_list_phase_t;

//列表动画状态
typedef struct{
    menu_list_phase_t phase;
    //当前菜单行距，进入时0-16，退出时16-0
    float lineSpacing;
    float lineSpacingTarget;
    //菜单内容纵向偏移，列表滚动时平滑变化
    float contentY;
    float contentTargetY;
    //圆角选择框的纵坐标
    float cursorY;
    float cursorTargetY; 
    //圆角选择框宽度，根据当前选中字符串自动计算
    float cursorWidth;
    float cursorTargetWidth;
    //右侧滚动进度条高度
    float barY;
    float barTargetY;
} menu_animation_t;

/* 一级磁贴菜单动画状态。 */
typedef struct{
    bool ready;

    float iconSpacing;
    float iconSpacingTarget;

    float iconOffsetX;
    float iconOffsetTargetX;

    float iconY;
    float iconTargetY;

    float indicatorWidth;
    float indicatorTargetWidth;

    float titleY;
    float titleTargetY;
} menu_tile_animation_t;

#endif /* UI_MENU_INTERNAL_H */