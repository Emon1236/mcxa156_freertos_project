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

#include "UI/menu.h"
#include "UI/menu_internal.h"
#include "UI/ui_settings.h"
#include "UI/ui_assets.h"
#include "UI/ui_animation.h"
#include "UI/ui_display_port.h"

// #include "bsp/bsp_4oleds.h"
#include "bsp/bsp_buzzer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "u8g2.h"

/* 静态变量声明*/
static menu_animation_t s_menuAnimation;
static menu_tile_animation_t s_tileAnimation;

static menu_frame_t s_menuStack[MENU_MAX_DEPTH];
static uint8_t s_menuDepth;
static menu_view_t s_currentView;

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

static void menu_tile_init(void);
static bool menu_tile_move(int8_t direction);
static bool menu_tile_update(uint32_t deltaTimeMs);
static void menu_render_tile(void);

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

//一级图标菜单，对应M_MAIN
static const menu_item_t s_rootItems[] = {
    {
        .label = "Sleep",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "Editor",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "Volt",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "Setting",
        .getLabel = NULL,
        .childPage = &s_settingsPage,
        .action = NULL
    }
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
//字体配置函数
static void menu_apply_font(u8g2_t *u8g2){
    const app_settings_t *settings = APP_Settings_Get();

    if (settings->font == APP_FONT_SMALL)
    {
        u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    }
    else
    {
        u8g2_SetFont(u8g2, u8g2_font_8x13_tf);
    }

    /*
     * 当前菜单使用文字左上角坐标。
     * 设置后，DrawStr的y可以继续理解为文字顶部。
     */
    u8g2_SetFontPosTop(u8g2);

    /*
     * 透明字体不会主动清除字符背景，
     * 适合先绘制文字再使用XOR光标。
     */
    u8g2_SetFontMode(u8g2, 1U);
    u8g2_SetDrawColor(u8g2, 1U);
}

/* 计算当前字体下一行占用的总高度。 */
static uint8_t menu_get_line_height(void){
    if (APP_Settings_Get()->font == APP_FONT_SMALL){
        return 12U;
    }
    return 16U;
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
        /* 已经位于M_MAIN，不能继续返回。 */
        return false;
    }

    s_menuDepth--;
    menu_synchronize_window(menu_current_frame());
    if(s_menuDepth == 0U){
        menu_tile_init();
    }
    else{
        menu_reset_animation();
    }
    // menu_reset_animation();
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
    menu_frame_t *frame = menu_current_frame();
    u8g2_t *u8g2 = UI_Display_GetContext();
    const char *label;
    uint16_t width;

    if ((frame->page == NULL) || (frame->page->itemCount == 0U)){
        return 0.0f;
    }

    label = menu_get_item_label(&frame->page->items[frame->selectedIndex]);

    if (label == NULL){
        return 0.0f;
    }

    menu_apply_font(u8g2);

    width = u8g2_GetStrWidth(u8g2, label);
    width += MENU_TEXT_X * 2U;

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
//初始化一级菜单动画：图标起始重叠并位于屏幕上方，随后横向展开向下滑入
static void menu_tile_init(void){
    menu_frame_t *frame = &s_menuStack[0];

    if (frame->selectedIndex >= s_rootPage.itemCount){
        frame->selectedIndex = 0U;
    }    
    s_tileAnimation.ready = false;
    //图标先重叠，然后逐渐展开到36像素间距
    s_tileAnimation.iconSpacing = 0.0f;
    s_tileAnimation.iconSpacingTarget = MENU_TILE_ICON_SPACING;
    //保证当前选中图标展开后位于屏幕中间，不从0开始，避免返回主菜单错误地居中第一项
    s_tileAnimation.iconOffsetTargetX = -(float)frame->selectedIndex * MENU_TILE_ICON_SPACING;
    s_tileAnimation.iconOffsetX = s_tileAnimation.iconOffsetTargetX;
    //图标从屏幕上方滑到y=0
    s_tileAnimation.iconY = -(float)UI_MAIN_ICON_HEIGHT;
    s_tileAnimation.iconTargetY = 0.0f;
    //左侧指示器从宽0展开到7
    s_tileAnimation.indicatorWidth = 0.0f;
    s_tileAnimation.indicatorTargetWidth = MENU_TILE_INDICATOR_WIDTH;
    //标题从屏幕下方向上滑入
    s_tileAnimation.titleY = MENU_TILE_TITLE_START_Y;
    s_tileAnimation.titleTargetY = MENU_TILE_TITLE_TARGET_Y;
}
//移动一级菜单
static bool menu_tile_move(int8_t direction){
    menu_frame_t *frame = &s_menuStack[0];
    uint8_t count = frame->page->itemCount;

    if ((count == 0U) || (direction == 0)){
        return false;
    }
    //首次图标展开前禁止移动
    if (!s_tileAnimation.ready){
        return false;
    }
    //根据方向更新选中索引，支持循环滚动行为：sleep向前选择会跳到setting。。。
    if (direction > 0){
        frame->selectedIndex = (uint8_t)((frame->selectedIndex + 1U) % count);
    }
    else{
        frame->selectedIndex = (uint8_t)((frame->selectedIndex + count - 1U) % count);
    }
    //整组移动，保证选中图表位于屏幕中央
    //同时，动画没结束时再次按键只改目标位置，当前值继续平滑接近目标
    s_tileAnimation.iconOffsetTargetX = -(float)frame->selectedIndex * MENU_TILE_ICON_SPACING;
    //标题上滑和指示器展开效果：每次改变选择时，把标题和指示器重新放回隐藏位置
    s_tileAnimation.indicatorWidth = 0.0f;
    s_tileAnimation.titleY = MENU_TILE_TITLE_START_Y;

    return true;
}
//更新一级菜单动画状态，返回true表示动画位置发生变化
static bool menu_tile_update(uint32_t deltaTimeMs){
    bool changed = false;

    changed |= UI_Animation_Approach(&s_tileAnimation.iconSpacing, s_tileAnimation.iconSpacingTarget,
        deltaTimeMs, MENU_TILE_ENTRY_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_tileAnimation.iconOffsetX, s_tileAnimation.iconOffsetTargetX,
        deltaTimeMs, MENU_TILE_ICON_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_tileAnimation.iconY, s_tileAnimation.iconTargetY,
        deltaTimeMs, MENU_TILE_ENTRY_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_tileAnimation.indicatorWidth, s_tileAnimation.indicatorTargetWidth,
        deltaTimeMs, MENU_TILE_INDICATOR_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_tileAnimation.titleY, s_tileAnimation.titleTargetY,
        deltaTimeMs, MENU_TILE_TITLE_RESPONSE_MS);

    /*
     * 参考工程只在初始图标间距展开完成后，
     * 才允许旋转选择一级菜单。
     */
    if ((!s_tileAnimation.ready) && (s_tileAnimation.iconSpacing == s_tileAnimation.iconSpacingTarget)){
        s_tileAnimation.ready = true;
    }

    return changed;    
}
//绘制一级菜单，使用u8g2 C接口；动画计算放在Menu_Update中，本函数只根据当前动画值绘制一帧
static void menu_render_tile(void){
    menu_frame_t *frame = &s_menuStack[0];
    u8g2_t *u8g2 = UI_Display_GetContext();

    const char *title;
    uint8_t count;
    uint8_t indicatorWidth;
    uint16_t titleWidth;
    uint16_t availableTitleWidth;

    int16_t titleX;
    int16_t titleY;
    int16_t iconY;
    int16_t iconBaseX;

    UI_Display_Clear();

    count = frame->page->itemCount;
    if (count > UI_MAIN_ICON_COUNT){
        /*
         * 防止菜单项数量大于图标数量时越界读取。
         * 当前M_MAIN菜单项和图标数量均为4。
         */
        count = UI_MAIN_ICON_COUNT;
    }

    if ((count == 0U) || (frame->selectedIndex >= count)){
        return;
    }

    /*
     * WouoUI磁贴标题使用18像素粗体。
     * DrawStr的y坐标按照字体基线解释。
     */
    u8g2_SetFont(u8g2, u8g2_font_helvB18_tr);
    u8g2_SetFontPosBaseline(u8g2);
    u8g2_SetFontMode(u8g2, 1U);
    u8g2_SetDrawColor(u8g2, 1U);

    title = menu_get_item_label(&frame->page->items[frame->selectedIndex]);

    if (title == NULL){
        title = "";
    }
    //标题在左侧7像素指示器以外的区域居中
    titleWidth = u8g2_GetStrWidth(u8g2, title);

    availableTitleWidth = UI_DISPLAY_WIDTH - (uint16_t)MENU_TILE_INDICATOR_WIDTH;

    if (titleWidth >= availableTitleWidth){
        titleX = (int16_t)MENU_TILE_INDICATOR_WIDTH;
    }
    else{
        titleX = (int16_t)MENU_TILE_INDICATOR_WIDTH + (int16_t)((availableTitleWidth - titleWidth) / 2U);
    }

    titleY = menu_float_to_int16(s_tileAnimation.titleY);

    u8g2_DrawStr(u8g2, (u8g2_uint_t)titleX, (u8g2_uint_t)titleY, title);

    //绘制标题左侧的竖向指示器。
    if (s_tileAnimation.indicatorWidth <= 0.0f){
        indicatorWidth = 0U;
    }
    else if (s_tileAnimation.indicatorWidth >= MENU_TILE_INDICATOR_WIDTH){
        indicatorWidth = (uint8_t)MENU_TILE_INDICATOR_WIDTH;
    }
    else{
        indicatorWidth = (uint8_t)(s_tileAnimation.indicatorWidth + 0.5f);
    }

    if (indicatorWidth > 0U){
        u8g2_DrawBox(u8g2, MENU_TILE_INDICATOR_X, MENU_TILE_INDICATOR_Y, indicatorWidth, MENU_TILE_INDICATOR_HEIGHT);
    }

    //图标组的静态中心位置：(128 - 30) / 2 = 49。
    iconBaseX = (int16_t)((UI_DISPLAY_WIDTH - UI_MAIN_ICON_WIDTH) / 2U);

    iconY = menu_float_to_int16(s_tileAnimation.iconY);

    /*
     * 每个图标位置由三部分组成：
     *
     * 屏幕中心
     * + 整组图标偏移量
     * + 当前图标序号 × 图标间距
     */
    for (uint8_t index = 0U; index < count; index++){
        float animatedX;
        int16_t iconX;

        animatedX = s_tileAnimation.iconOffsetX + (float)index * s_tileAnimation.iconSpacing;

        iconX = iconBaseX + menu_float_to_int16(animatedX);

        /*
         * 数据来自XBM数组，因此必须使用DrawXBMP，
         * 不能改成DrawBitmap。
         */
        u8g2_DrawXBMP(u8g2, (u8g2_uint_t)iconX, (u8g2_uint_t)iconY, UI_MAIN_ICON_WIDTH, UI_MAIN_ICON_HEIGHT, g_uiMainIcons[index]);
    }    
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
    uint8_t count;
    uint8_t visibleCount;
    uint8_t lineHeight;

    int16_t contentOffsetY;
    int16_t firstRelativeRow;
    int16_t lastRelativeRow;

    int16_t cursorY;
    uint16_t cursorWidth;

    u8g2_t *u8g2 = UI_Display_GetContext();

    UI_Display_Clear();
    menu_apply_font(u8g2);
    const app_settings_t *settings = APP_Settings_Get();
    menu_frame_t *frame = menu_current_frame();

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

        u8g2_DrawStr(u8g2, MENU_TEXT_X, textY, label);
    }
    //将光标动画中的浮点坐标转换成实际绘图坐标
    cursorY = menu_float_to_int16(s_menuAnimation.cursorY);
    //动画刚开始时宽度可能为0或者非常接近0
    if (s_menuAnimation.cursorWidth <= 0.0f){
        cursorWidth = 0U;
    }
    else{
        cursorWidth = (uint16_t)s_menuAnimation.cursorWidth+0.5f;
    }
    if (cursorWidth > UI_DISPLAY_WIDTH){//防止光标宽度超过屏幕
        cursorWidth = UI_DISPLAY_WIDTH;
    }
    if (cursorWidth == 0U){//宽度为0时不绘制光标
        return;
    }

    //所有文字绘制完成以后再绘制光标，这样反色光标可以同时反转背景和文字。
    if (settings->cursorStyle == APP_CURSOR_INVERSE){
        /*
        * DrawColor=2表示XOR。
        * 文字已经绘制，因此矩形区域会同时反转背景和文字。
        */
        u8g2_SetDrawColor(u8g2, 2U);
        u8g2_DrawBox(u8g2, 0, cursorY, cursorWidth, lineHeight);
        u8g2_SetDrawColor(u8g2, 1U);
    }
    else{
        u8g2_DrawFrame(u8g2, 0, cursorY, cursorWidth, lineHeight);
    }
}

/* 显示尚未实现功能的提示页面。 */
static void menu_render_notice(void){
    const char *message = "COMING SOON";
    const char *backText = "B3/B4 RETURN";

    u8g2_t *u8g2 = UI_Display_GetContext();

    uint16_t messageWidth;
    uint16_t backWidth;

    int16_t messageX;
    int16_t backX;

    UI_Display_Clear();
    menu_apply_font(u8g2);

    messageWidth = u8g2_GetStrWidth(u8g2, message);
    backWidth = u8g2_GetStrWidth(u8g2, backText);

    messageX = (messageWidth < UI_DISPLAY_WIDTH) ? (int16_t)((UI_DISPLAY_WIDTH - messageWidth) / 2U) : 0;
    backX = (backWidth < UI_DISPLAY_WIDTH) ? (int16_t)((UI_DISPLAY_WIDTH - backWidth) / 2U) : 0;

    u8g2_DrawStr(u8g2, messageX, 24, message);
    u8g2_DrawStr(u8g2, backX, 50, backText);
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
    if (APP_Settings_Get()->font == APP_FONT_SMALL){
        return "FONT: SMALL";
    }

    return "FONT: LARGE";
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
    //一级菜单动画初始化
    menu_tile_init();
}

bool Menu_HandleKey(menu_key_t key){
    /* 在修改KEY SOUND设置之前播放声音。
     * 因此从ON改成OFF时，这一次仍然会响；从OFF改成ON时，从下一次按键开始响。
     */
    menu_play_key_tone();

    if (s_currentView == MENU_VIEW_NOTICE){
        if ((key == MENU_KEY_OK) || (key == MENU_KEY_BACK)){
            s_currentView = MENU_VIEW_LIST;
            if (s_menuDepth == 0U){
                menu_tile_init();   //从提示页面返回M_MAIN
            }else{
                menu_reset_animation();//提示页面返回时重置动画
            }
            return true;
        }
        return false;
    }

    switch (key){
        case MENU_KEY_UP:
            if(s_menuDepth ==0U){
                return menu_tile_move(-1);  //按键1选择前一个图标
            }
            menu_move_up();
            return true;

        case MENU_KEY_DOWN:
            if(s_menuDepth ==0U){
                return menu_tile_move(1);   //按键2选择下一个图标
            }
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
    //根页面对应M_MAIN，使用一级菜单动画
    if(s_menuDepth == 0U){
        return menu_tile_update(deltaTimeMs);
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
    else if(s_menuDepth == 0U){
        menu_render_tile(); //一级根菜单用UI图标样式
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
