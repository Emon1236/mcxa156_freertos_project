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

// #include "bsp/bsp_4oleds.h"
#include "bsp/bsp_buzzer.h"
#include "bsp/bsp_oled_12864.h"
#include "bsp/bsp_oled_data.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app/ui_animation.h"
#include "app/ui_display_port.h"

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

//新增动画状态
typedef struct{
    //菜单内容的临时纵向偏移，＋表示内容向下，-表示内容向上
    float currentY; //当前动画位置
    float currentTargetY;  //目标动画位置
    //光标纵坐标当前位置和目标位置
    float cursorY;
    float cursorTargetY;
    //光标矩形当前宽度和目标宽度
    float cursorWidth;
    float cursorTargetWidth;
} menu_animation_t;

//初始化一个静态变量结构体，菜单动画状态
static menu_animation_t s_menuAnimation; 

/* 静态函数声明区 */
static void menu_action_font(void);
static void menu_action_key_sound(void);
static void menu_action_cursor(void);
static void menu_action_open_notice(void);

static const char *menu_get_font_label(void);
static const char *menu_get_key_sound_label(void);
static const char *menu_get_cursor_label(void);

/************************************************************/
static const char *menu_get_item_label(const menu_item_t *item);

static void menu_set_animation_targets(void);//设置动画目标值
static void menu_reset_animation(void);     //重置动画状态

static int16_t menu_float_to_int16(float value);    //将浮点数转换为int16_t，避免直接强制类型转换可能导致的溢出问题
static uint8_t menu_wrap_item_index(int32_t index, uint8_t count);//将菜单项索引限制在有效范围内，支持循环滚动


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
/****************************************/
//优化边界光标选择逻辑

//返回当前字体状态下，虚拟屏幕能够容纳的菜单行数，不会被菜单实际项目数量影响
static uint8_t menu_get_screen_capacity(void){
    uint8_t lineHeight = menu_get_line_height();
    if (lineHeight == 0U){
        return 0U;
    }
    // return (uint8_t)(OLED4_HEIGHT / lineHeight);
    return (uint8_t)(UI_DISPLAY_HEIGHT / lineHeight);
}

//区分两个概念：screen_capacity时屏幕能放下多少行，visibleCount是当前实际显示的菜单数量
static uint8_t menu_get_visible_count(const menu_page_t *page){
    uint8_t capacity;
    //uint8_t lineHeight;

    if ((page == NULL) || (page->itemCount == 0U)){
        return 0U;
    }

    //lineHeight = menu_get_line_height();
    capacity = menu_get_screen_capacity();

    if (page->itemCount < capacity){
        return page->itemCount;
    }
    return capacity;
}

/*
 * 字体改变后，屏幕能显示的行数也会改变，保证光标仍位于有效的可见行内。
 优化翻页滚动逻辑
 */
static void menu_synchronize_window(menu_frame_t *frame){
    uint8_t count;//菜单项总数
    uint8_t capacity;//屏幕容量

    if ((frame == NULL) || (frame->page == NULL)){
        return;
    }

    count = frame->page->itemCount;
    capacity = menu_get_screen_capacity();
    if (count == 0U || capacity == 0U){//没有菜单项，重置所有状态
        frame->selectedIndex = 0U;
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = 0U;
        return;
    }
    if(frame->selectedIndex >= count){//防止异常索引
        frame->selectedIndex = 0U;
    }
    if (count <= capacity){//菜单不需要滚动时，强制恢复自然排列
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = frame->selectedIndex;
        return;
    }
    if (frame->cursorRow >= capacity){//菜单需要滚动时，保证光标位于屏幕范围内
        frame->cursorRow = (uint8_t)(capacity - 1U);
    }

    /*
     * 保持下面的关系：
     * selectedIndex = firstVisibleIndex + cursorRow
     * 因为菜单可以首尾循环，所以需要进行取模。
     */
    frame->firstVisibleIndex =(uint8_t)((frame->selectedIndex + count - frame->cursorRow) % count);
}

/* 向上移动一次。 */
static void menu_move_up(void){
    menu_frame_t *frame = menu_current_frame();
    uint8_t count = frame->page->itemCount;
    uint8_t capacity;
    uint8_t lineHeight;

    if (count == 0U)
    {
        return;
    }

    capacity = menu_get_screen_capacity();

    if (capacity == 0U)
    {
        return;
    }

    lineHeight = menu_get_line_height();

    frame->selectedIndex =
        (uint8_t)((frame->selectedIndex + count - 1U) % count);

    /*
     * 菜单数量不超过屏幕容量时，菜单内容不滚动，
     * 只移动光标。
     */
    if (count <= capacity)
    {
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = frame->selectedIndex;

        menu_set_animation_targets();
        return;
    }

    if (frame->cursorRow > 0U)
    {
        /* 光标尚未到达第一行，只移动光标。 */
        frame->cursorRow--;
    }
    else
    {
        /*
         * 光标已经位于第一行。
         * firstVisibleIndex向前移动一项。
         *
         * 新菜单初始向上偏移一行，然后逐渐回到0，
         * 看起来就是菜单内容向下滑动。
         */
        frame->firstVisibleIndex =
            (uint8_t)((frame->firstVisibleIndex + count - 1U) % count);

        s_menuAnimation.currentY -= (float)lineHeight;
    }

    menu_set_animation_targets();
}
/* 向下移动一次。 */
static void menu_move_down(void){
    menu_frame_t *frame = menu_current_frame();
    uint8_t count = frame->page->itemCount;
    uint8_t capacity;
    uint8_t lineHeight;

    if (count == 0U)
    {
        return;
    }

    capacity = menu_get_screen_capacity();

    if (capacity == 0U)
    {
        return;
    }

    lineHeight = menu_get_line_height();

    frame->selectedIndex =
        (uint8_t)((frame->selectedIndex + 1U) % count);

    /*
     * 菜单项目没有超过屏幕容量时，
     * firstVisibleIndex永远保持为0。
     */
    if (count <= capacity)
    {
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = frame->selectedIndex;

        menu_set_animation_targets();
        return;
    }

    if ((frame->cursorRow + 1U) < capacity)
    {
        /* 光标还没有到达最后一行，只移动光标。 */
        frame->cursorRow++;
    }
    else
    {
        /*
         * 光标已经位于最后一行。
         * firstVisibleIndex向后移动一项。
         *
         * 新菜单初始向下偏移一行，然后逐渐回到0，
         * 看起来就是菜单内容向上滑动。
         */
        frame->firstVisibleIndex =
            (uint8_t)((frame->firstVisibleIndex + 1U) % count);

        s_menuAnimation.currentY += (float)lineHeight;
    }

    menu_set_animation_targets();
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

    menu_reset_animation();
}

/* 返回上一级菜单。 */
static bool menu_go_back(void){
    if (s_menuDepth == 0U){
        /* 已经位于一级菜单，不能继续返回。 */
        return false;
    }

    s_menuDepth--;
    menu_synchronize_window(menu_current_frame());
    menu_reset_animation();

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

static float menu_get_cursor_target_width(void){
    const app_settings_t *settings = APP_Settings_Get();
    menu_frame_t *frame = menu_current_frame();
    const char *label;
    uint32_t width;

    if ((frame->page == NULL) || (frame->page->itemCount == 0U)){
        return 0.0f;
    }

    label = menu_get_item_label(&frame->page->items[frame->selectedIndex]);

    if (label == NULL){
        return 0.0f;
    }

    //左侧文字留白为MENU_TEXT_X。为了让右侧也有一点留白，所以再加两倍MENU_TEXT_X。
    width = (uint32_t)strlen(label) * OLED_FONT_WIDTH(settings->font);

    width += (uint32_t)MENU_TEXT_X * 2U;
    if (width > UI_DISPLAY_WIDTH){
        width = UI_DISPLAY_WIDTH;
    }

    return (float)width;    
}
static void menu_set_animation_targets(void)
{
    menu_frame_t *frame = menu_current_frame();
    uint8_t lineHeight = menu_get_line_height();

    /*
     * 菜单滚动动画最终总是回到0偏移。
     * 按键发生滚动时会临时改变currentY。
     */
    s_menuAnimation.currentTargetY = 0.0f;

    s_menuAnimation.cursorTargetY =
        (float)((uint16_t)frame->cursorRow * lineHeight);

    s_menuAnimation.cursorTargetWidth =
        menu_get_cursor_target_width();
}

static void menu_reset_animation(void)
{
    menu_set_animation_targets();

    s_menuAnimation.currentY = s_menuAnimation.currentTargetY;
    s_menuAnimation.cursorY = s_menuAnimation.cursorTargetY;
    s_menuAnimation.cursorWidth = s_menuAnimation.cursorTargetWidth;
}

static int16_t menu_float_to_int16(float value)
{
    if (value >= 0.0f)
    {
        return (int16_t)(value + 0.5f);
    }

    return (int16_t)(value - 0.5f);
}

static uint8_t menu_wrap_item_index(int32_t index, uint8_t count)
{
    int32_t wrappedIndex;

    if (count == 0U)
    {
        return 0U;
    }

    wrappedIndex = index % count;

    if (wrappedIndex < 0)
    {
        wrappedIndex += count;
    }

    return (uint8_t)wrappedIndex;
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
        //修改字体后，每页可显示行数可能发生变化。
        //因此需要重新校正窗口和动画坐标
        menu_synchronize_window(menu_current_frame());
        menu_reset_animation();
    }
}

/* 显示普通菜单列表。 */
static void menu_render_list(void){
    const app_settings_t *settings = APP_Settings_Get();
    menu_frame_t *frame = menu_current_frame();

    uint8_t count;
    uint8_t visibleCount;
    uint8_t lineHeight;

    int16_t contentOffsetY;
    int16_t firstRelativeRow;
    int16_t lastRelativeRow;

    int16_t cursorY;
    uint16_t cursorWidth;

    UI_Display_Clear();

    count = frame->page->itemCount;

    if (count == 0U){
        return;
    }

    visibleCount = menu_get_visible_count(frame->page);
    lineHeight = menu_get_line_height();

    if ((visibleCount == 0U) || (lineHeight == 0U)){
        return;
    }

    contentOffsetY = menu_float_to_int16(s_menuAnimation.currentY);

    if (count <= visibleCount){
    //菜单不需要滚动。所有项目始终按照自然顺序排列。
        firstRelativeRow = 0;
        lastRelativeRow = (int16_t)count - 1;
    }
    else{
        int16_t extraRows = 0;
        //滚动期间要额外绘制屏幕边缘外的菜单项，否则菜单进入屏幕时会突然出现。
        if (contentOffsetY != 0){
            int16_t absoluteOffset;
            absoluteOffset = contentOffsetY;

            if (absoluteOffset < 0){
                absoluteOffset = (int16_t)-absoluteOffset;
            }

            extraRows = (int16_t)(absoluteOffset / lineHeight + 1);

            if (extraRows > count){
                extraRows = count;
            }
        }

        firstRelativeRow = 0;
        lastRelativeRow = (int16_t)visibleCount - 1;

        if (contentOffsetY > 0){
            /* 菜单向上滚动时，需要绘制上方的旧项目。 */
            firstRelativeRow = (int16_t)(firstRelativeRow - extraRows);
        }
        else if (contentOffsetY < 0){
            /* 菜单向下滚动时，需要绘制下方的旧项目。 */
            lastRelativeRow = (int16_t)(lastRelativeRow + extraRows);
        }
    }

    /*
     * 先绘制全部菜单文字。
     * 超出屏幕的部分由ShowStringClipped自动裁剪。
     */
    for (int16_t relativeRow = firstRelativeRow; relativeRow <= lastRelativeRow; relativeRow++){
        uint8_t itemIndex;
        int16_t textY;
        const char *label;

        itemIndex = menu_wrap_item_index((int32_t)frame->firstVisibleIndex + relativeRow, count);

        label = menu_get_item_label(&frame->page->items[itemIndex]);

        textY = (int16_t)(relativeRow * lineHeight + contentOffsetY + 1);

        BSP_OLED_ShowStringClipped((int16_t)MENU_TEXT_X, textY, label, settings->font);
    }
    //所有文字绘制完成以后再绘制光标，这样反色光标可以同时反转背景和文字。
    cursorY = menu_float_to_int16(s_menuAnimation.cursorY);

    if (s_menuAnimation.cursorWidth <= 0.0f){
        cursorWidth = 0U;
    }
    else{
        cursorWidth = (uint16_t)(s_menuAnimation.cursorWidth + 0.5f);
    }

    if (cursorWidth > UI_DISPLAY_WIDTH){
        cursorWidth = UI_DISPLAY_WIDTH;
    }

    if (cursorWidth == 0U){
        return;
    }

    if (settings->cursorStyle == APP_CURSOR_INVERSE){
        BSP_OLED_InvertRectangle(0, cursorY, cursorWidth, lineHeight);
    }
    else{
        BSP_OLED_DrawRectangleClipped( 0, cursorY, cursorWidth, lineHeight);
    }
}

/* 显示尚未实现功能的提示页面。 */
static void menu_render_notice(void){
    const app_settings_t *settings = APP_Settings_Get();

    const char *message = "COMING SOON";
    const char *backText = "B3/B4 RETURN";

    uint16_t fontWidth;
    uint16_t fontHeight;
    uint16_t messageWidth;
    uint16_t backWidth;

    int16_t messageX;
    int16_t messageY;
    int16_t backX;
    int16_t backY;

    fontWidth = OLED_FONT_WIDTH(settings->font);
    fontHeight = OLED_FONT_HEIGHT(settings->font);

    messageWidth = (uint16_t)strlen(message) * fontWidth;

    backWidth = (uint16_t)strlen(backText) * fontWidth;
    //三元运算符，如果文字宽度小于屏幕宽度，则居中显示，否则从最左侧开始显示。
    messageX = (messageWidth < UI_DISPLAY_WIDTH) ? (int16_t)((UI_DISPLAY_WIDTH - messageWidth) / 2U) : 0;

    backX = (backWidth < UI_DISPLAY_WIDTH) ? (int16_t)((UI_DISPLAY_WIDTH - backWidth) / 2U) : 0;

    messageY = (int16_t)((UI_DISPLAY_HEIGHT - fontHeight) / 2U);

    backY = (int16_t)(UI_DISPLAY_HEIGHT - fontHeight);

    UI_Display_Clear();

    BSP_OLED_ShowStringClipped(messageX,messageY,message,settings->font);

    BSP_OLED_ShowStringClipped(backX,backY,backText,settings->font);
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
    //新增动画初始化
    menu_reset_animation();
}

bool Menu_HandleKey(menu_key_t key){
    /* 在修改KEY SOUND设置之前播放声音。
     * 因此从ON改成OFF时，这一次仍然会响；从OFF改成ON时，从下一次按键开始响。
     */
    menu_play_key_tone();

    if (s_currentView == MENU_VIEW_NOTICE){
        if ((key == MENU_KEY_OK) || (key == MENU_KEY_BACK)){
            s_currentView = MENU_VIEW_LIST;
            //提示页面返回时重置动画
            menu_reset_animation();
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

bool Menu_Update(uint32_t deltaTimeMs){
    bool changed = false;
    //提示页面没有菜单滚动动画。
    if (s_currentView != MENU_VIEW_LIST){
        return false;
    }
    // |=是按位或赋值运算符，表示如果任何一个动画状态发生了变化，changed就会被置为true。
    changed |= UI_Animation_Approach(&s_menuAnimation.currentY, s_menuAnimation.currentTargetY, deltaTimeMs, MENU_SCROLL_RESPONSE_MS);
    changed |= UI_Animation_Approach(&s_menuAnimation.cursorY, s_menuAnimation.cursorTargetY, deltaTimeMs, MENU_CURSOR_RESPONSE_MS);
    changed |= UI_Animation_Approach(&s_menuAnimation.cursorWidth, s_menuAnimation.cursorTargetWidth, deltaTimeMs, MENU_CURSOR_WIDTH_RESPONSE_MS);

    return changed;    
}

void Menu_Render(void){
    if (s_currentView == MENU_VIEW_NOTICE){
        menu_render_notice();
    }
    else{
        menu_render_list();
    }
    // /*
    //  * 所有绘图完成后只刷新一次，
    //  * 避免刷新过程中出现闪烁。
    //  */
    // BSP_4OLEDs_Refresh();
}
