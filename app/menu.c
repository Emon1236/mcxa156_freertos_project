/*
1. 菜单滚动状态设计思路：
selectedIndex 当前真正选中的菜单项
firstVisibleIndex 屏幕第一行显示的菜单项
cursorIndex 光标位于屏幕第几行
例如使用8x16字体，一行占16+2=18个像素，128像素可显示7行；光标没到最后一行，cursorRow++
光标到最后一行，firstVisibleIndex++,光标不动，选中3D后继续向下，选中SETTINGS。

2.按键功能分配
按键1，向上；按键2，向下；按键3，确认；按键4，返回
*/

#include "app/menu.h"
#include "app/app_settings.h"

#include "bsp/bsp_4oleds.h"
#include "bsp/bsp_buzzer.h"
#include "bsp/bsp_oled_data.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

//最多允许的菜单层数
#define MENU_MAX_DEPTH 4U
//菜单左侧留出空白
#define MENU_TEXT_X 4U
//文字上下各留一个像素
#define MENU_LINE_PADDING 2U

//按键音参数
#define MENU_KEY_TONE_HZ 2000U
#define MENU_KEY_TONE_MS 50U

typedef struct menu_page menu_page_t;
typedef void (*menu_action_t)(void);
typedef const char *(*menu_label_getter_t)(void);

//菜单项功能结构体
typedef struct{
    const char *label; //菜单项显示文本
    menu_label_getter_t getLabel; //菜单项显示文本获取函数
    const menu_page_t *childPage; //子菜单页指针
    menu_action_t action; //菜单项动作函数
}menu_item_t;
/* 一个菜单页面由菜单项数组和菜单项数量组成。 */
struct menu_page{
    const menu_item_t *items;
    uint8_t itemCount;
};
// 菜单框架状态结构体
typedef struct{
    const menu_page_t *page; //当前菜单页指针

    uint8_t selectedIndex;  //当前选中的菜单项索引
    uint8_t firstVisibleIndex;//当前屏幕第一行显示的菜单项索引
    uint8_t cursorRow;      //光标位于屏幕第几行
} menu_frame_t;

typedef enum{
    MENU_VIEW_LIST = 0U,    //菜单列表视图
    MENU_VIEW_NOTICE        //通知视图
} menu_view_t;

/* 函数声明。 */
static void menu_action_font(void);
static void menu_action_key_sound(void);
static void menu_action_cursor(void);
static void menu_action_open_notice(void);

static const char *menu_get_font_label(void);
static const char *menu_get_key_sound_label(void);
static const char *menu_get_cursor_label(void);


/* SETTINGS页面需要先声明，一级菜单才能引用它。 */
static const menu_page_t s_settingsPage;

/* SETTINGS二级菜单。 */
static const menu_item_t s_settingsItems[] = {
    {
        .label = NULL,
        .getLabel = menu_get_font_label,
        .childPage = NULL,
        .action = menu_action_font
    },
    {
        .label = NULL,
        .getLabel = menu_get_key_sound_label,
        .childPage = NULL,
        .action = menu_action_key_sound
    },
    {
        .label = NULL,
        .getLabel = menu_get_cursor_label,
        .childPage = NULL,
        .action = menu_action_cursor
    }
};

static const menu_page_t s_settingsPage = {
    .items = s_settingsItems,
    .itemCount = (uint8_t)(sizeof(s_settingsItems) / sizeof(s_settingsItems[0]))
};

/*
 * 一级菜单。
 * 其他菜单目前全部执行“即将上线”提示功能。
 */
static const menu_item_t s_rootItems[] = {
    {"SETTINGS",         NULL, &s_settingsPage, NULL},  //SETTINGS进入二级菜单
    {"TEMP_SENSOR",      NULL, NULL, menu_action_open_notice},
    {"IMG_TRANSMISSION", NULL, NULL, menu_action_open_notice},
    {"CLOCK",            NULL, NULL, menu_action_open_notice},
    {"GAME",             NULL, NULL, menu_action_open_notice},
    {"WEATHER",          NULL, NULL, menu_action_open_notice},
    {"MUSIC",            NULL, NULL, menu_action_open_notice},
    {"WIFI",             NULL, NULL, menu_action_open_notice},
    {"CTRL_PANEL",       NULL, NULL, menu_action_open_notice},
    {"3D",               NULL, NULL, menu_action_open_notice}
};

static const menu_page_t s_rootPage = {
    .items = s_rootItems,
    .itemCount = (uint8_t)(sizeof(s_rootItems) / sizeof(s_rootItems[0]))
};

static menu_frame_t s_menuStack[MENU_MAX_DEPTH];//菜单栈。最大4层，0是一级菜单，1是SETTINGS。
static uint8_t s_menuDepth; //当前处于第几层菜单
static menu_view_t s_currentView;   //当前显示菜单列表还是提示页面
//取得当前菜单层
static menu_frame_t *menu_current_frame(void)   {
    return &s_menuStack[s_menuDepth];
}

/* 计算当前字体下一行占用的总高度。 */
static uint8_t menu_get_line_height(void){
    const app_settings_t *settings = APP_Settings_Get();

    return (uint8_t)(OLED_FONT_HEIGHT(settings->font) + MENU_LINE_PADDING);
}

/* 计算当前屏幕最多能够显示多少行。 */
static uint8_t menu_get_visible_count(const menu_page_t *page){
    uint8_t capacity;
    uint8_t lineHeight;

    if ((page == NULL) || (page->itemCount == 0U)){
        return 0U;
    }

    lineHeight = menu_get_line_height();
    capacity = (uint8_t)(OLED4_HEIGHT / lineHeight);

    if (capacity > page->itemCount){
        capacity = page->itemCount;
    }
    return capacity;
}

/*
 * 字体改变后，屏幕能显示的行数也会改变，保证光标仍位于有效的可见行内。
 */
static void menu_synchronize_window(menu_frame_t *frame){
    uint8_t visibleCount;

    if ((frame == NULL) || (frame->page == NULL)){
        return;
    }

    visibleCount = menu_get_visible_count(frame->page);

    if (visibleCount == 0U){//没有菜单项，重置所有状态
        frame->selectedIndex = 0U;
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = 0U;
        return;
    }
    if (frame->cursorRow >= visibleCount){
        frame->cursorRow = (uint8_t)(visibleCount - 1U);
    }

    /*
     * 保持下面的关系：
     * selectedIndex = firstVisibleIndex + cursorRow
     * 因为菜单可以首尾循环，所以需要进行取模。
     */
    frame->firstVisibleIndex =(uint8_t)((frame->selectedIndex + frame->page->itemCount - frame->cursorRow) % frame->page->itemCount);
}

/* 向上移动一次。 */
static void menu_move_up(void){
    menu_frame_t *frame = menu_current_frame();
    uint8_t count = frame->page->itemCount;

    if (count == 0U){//没有菜单项，什么也不做
        return;
    }
    /* SETTINGS向上继续移动时，会变成3D。 */
    frame->selectedIndex = (uint8_t)((frame->selectedIndex + count - 1U) % count);
    if (frame->cursorRow > 0U){
        frame->cursorRow--; /* 光标还没到第一行，只移动光标。 */
    }
    else{//光标已经在第一行，光标保持不动，窗口内容向下滚动一项。
        frame->firstVisibleIndex = (uint8_t)((frame->firstVisibleIndex + count - 1U) % count);
    }
}
/* 向下移动一次。 */
static void menu_move_down(void){
    menu_frame_t *frame = menu_current_frame();
    uint8_t count = frame->page->itemCount;
    uint8_t visibleCount;

    if (count == 0U){
        return;
    }

    visibleCount = menu_get_visible_count(frame->page);//计算当前屏幕最多显示多少行
    /* 3D向下继续移动时，会变成SETTINGS。 */
    frame->selectedIndex = (uint8_t)((frame->selectedIndex + 1U) % count);

    if ((frame->cursorRow + 1U) < visibleCount){
        /* 光标还没到最后一行，只移动光标。 */
        frame->cursorRow++;
    }
    else{//光标已经在最后一行，光标保持不动，窗口内容向上滚动一项。
        frame->firstVisibleIndex = (uint8_t)((frame->firstVisibleIndex + 1U) % count);
    }
}

/* 进入下一级菜单。 */
static void menu_enter_page(const menu_page_t *page){
    menu_frame_t *newFrame;
    if (page == NULL){
        return;
    }

    if ((s_menuDepth + 1U) >= MENU_MAX_DEPTH){
        return;
    }

    s_menuDepth++;
    newFrame = menu_current_frame();

    newFrame->page = page;
    newFrame->selectedIndex = 0U;
    newFrame->firstVisibleIndex = 0U;
    newFrame->cursorRow = 0U;
}

/* 返回上一级菜单。 */
static bool menu_go_back(void){
    if (s_menuDepth == 0U){
        /* 已经位于一级菜单，不能继续返回。 */
        return false;
    }

    s_menuDepth--;
    menu_synchronize_window(menu_current_frame());

    return true;
}

/* 获得菜单项最终需要显示的文字。 */
static const char *menu_get_item_label(const menu_item_t *item)
{
    if (item->getLabel != NULL){
        return item->getLabel();
    }

    return item->label;
}

/* 执行当前选中的菜单项。 */
static void menu_activate_selected_item(void)
{
    menu_frame_t *frame = menu_current_frame();
    const menu_item_t *item;

    if (frame->page->itemCount == 0U){
        return;
    }

    item = &frame->page->items[frame->selectedIndex];

    if (item->childPage != NULL){
        menu_enter_page(item->childPage);
    }
    else if (item->action != NULL){
        item->action();
        //修改字体后，每页可显示行数可能发生变化，因此重新校正窗口。
        menu_synchronize_window(menu_current_frame());
    }
}

/* 显示普通菜单列表。 */
static void menu_render_list(void){
    const app_settings_t *settings = APP_Settings_Get();
    menu_frame_t *frame = menu_current_frame();

    uint8_t visibleCount;
    uint8_t lineHeight;
    uint8_t count;

    BSP_4OLEDs_Clear();

    count = frame->page->itemCount;
    visibleCount = menu_get_visible_count(frame->page);
    lineHeight = menu_get_line_height();

    for (uint8_t row = 0U; row < visibleCount; row++){
        uint8_t itemIndex;
        uint16_t lineY;
        const char *label;
        bool selected;

        itemIndex = (uint8_t)((frame->firstVisibleIndex + row) % count);

        lineY = (uint16_t)row * lineHeight;
        label = menu_get_item_label(&frame->page->items[itemIndex]);
        selected = (row == frame->cursorRow);

        if (selected && (settings->cursorStyle == APP_CURSOR_INVERSE)){
            /* 整个选中行填充为亮色。 */
            BSP_4OLEDs_FillRectangle(0U, lineY, OLED4_WIDTH, lineHeight, 1U);

            /* 使用反色字模在亮色背景上显示黑色文字。 */
            BSP_4OLEDs_ShowStringInverted(MENU_TEXT_X, (uint16_t)(lineY + 1U), label, settings->font);
        }
        else{
            if (selected && (settings->cursorStyle == APP_CURSOR_BORDER)){
                BSP_4OLEDs_DrawRectangle(0U, lineY, OLED4_WIDTH, lineHeight);
            }
            BSP_4OLEDs_ShowString(MENU_TEXT_X, (uint16_t)(lineY + 1U), label, settings->font);
        }
    }
}

/* 显示尚未实现功能的提示页面。 */
static void menu_render_notice(void){
    const app_settings_t *settings = APP_Settings_Get();
    const char *message = "COMING SOON";
    const char *backText = "B3/B4 TO RETURN";

    uint16_t messageWidth;
    uint16_t backWidth;
    uint16_t messageX;
    uint16_t backX;
    uint16_t messageY;

    messageWidth =(uint16_t)strlen(message) * OLED_FONT_WIDTH(settings->font);

    backWidth =(uint16_t)strlen(backText) * OLED_FONT_WIDTH(settings->font);

    messageX = (uint16_t)((OLED4_WIDTH - messageWidth) / 2U);
    backX = (uint16_t)((OLED4_WIDTH - backWidth) / 2U);

    messageY = (uint16_t)((OLED4_HEIGHT - OLED_FONT_HEIGHT(settings->font)) / 2U);

    BSP_4OLEDs_Clear();

    BSP_4OLEDs_ShowString(messageX, messageY, message, settings->font);

    if ((messageY + OLED_FONT_HEIGHT(settings->font) + 8U + OLED_FONT_HEIGHT(settings->font)) <= OLED4_HEIGHT){
        BSP_4OLEDs_ShowString(backX, (uint16_t)(messageY + OLED_FONT_HEIGHT(settings->font) + 8U), backText, settings->font);
    }
}

/* 根据当前设置播放按键音。 */
static void menu_play_key_tone(void){
    const app_settings_t *settings = APP_Settings_Get();

    if (settings->keySoundEnabled){
        BSP_Buzzer_PlayTone(MENU_KEY_TONE_HZ, MENU_KEY_TONE_MS);
    }
}

/* SETTINGS动态文字。 */
static const char *menu_get_font_label(void){
    if (APP_Settings_Get()->font == OLED_FONT_6X8){
        return "FONT: 6X8";
    }

    return "FONT: 8X16";
}

static const char *menu_get_key_sound_label(void){
    if (APP_Settings_Get()->keySoundEnabled){
        return "KEY SOUND: ON";
    }

    return "KEY SOUND: OFF";
}

static const char *menu_get_cursor_label(void){
    if (APP_Settings_Get()->cursorStyle == APP_CURSOR_INVERSE){
        return "CURSOR: INVERSE";
    }

    return "CURSOR: BORDER";
}

/* SETTINGS功能回调。 */
static void menu_action_font(void){
    APP_Settings_NextFont();
}

static void menu_action_key_sound(void){
    APP_Settings_ToggleKeySound();
}

static void menu_action_cursor(void){
    APP_Settings_NextCursorStyle();
}

/* 所有尚未完成的一级菜单暂时调用这个函数。 */
static void menu_action_open_notice(void){
    s_currentView = MENU_VIEW_NOTICE;
}

void Menu_Init(void){
    APP_Settings_Init();

    s_menuDepth = 0U;
    s_currentView = MENU_VIEW_LIST;

    s_menuStack[0].page = &s_rootPage;
    s_menuStack[0].selectedIndex = 0U;
    s_menuStack[0].firstVisibleIndex = 0U;
    s_menuStack[0].cursorRow = 0U;
}

bool Menu_HandleKey(menu_key_t key){
    /* 在修改KEY SOUND设置之前播放声音。
     * 因此从ON改成OFF时，这一次仍然会响；从OFF改成ON时，从下一次按键开始响。
     */
    menu_play_key_tone();

    if (s_currentView == MENU_VIEW_NOTICE){
        if ((key == MENU_KEY_OK) || (key == MENU_KEY_BACK)){
            s_currentView = MENU_VIEW_LIST;
            return true;
        }
        return false;
    }

    switch (key){
        case MENU_KEY_UP:
            menu_move_up();
            return true;

        case MENU_KEY_DOWN:
            menu_move_down();
            return true;

        case MENU_KEY_OK:
            menu_activate_selected_item();
            return true;

        case MENU_KEY_BACK:
            return menu_go_back();

        default:
            return false;
    }
}

void Menu_Render(void){
    if (s_currentView == MENU_VIEW_NOTICE){
        menu_render_notice();
    }
    else{
        menu_render_list();
    }
    /*
     * 所有绘图完成后只刷新一次，
     * 避免刷新过程中出现闪烁。
     */
    BSP_4OLEDs_Refresh();
}
