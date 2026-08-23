/*

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

static menu_frame_t s_menuStack[MENU_MAX_DEPTH];//菜单栈。最大4层，0是一级菜单，1是SETTINGS。
static uint8_t s_menuDepth; //当前处于第几层菜单
static menu_view_t s_currentView;   //当前显示菜单列表还是提示页面

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
static int16_t menu_float_to_int16(float value);    //将浮点数转换为int16_t，避免直接强制类型转换可能导致的溢出问题

static uint8_t menu_get_font_height(void);
static void menu_list_normalize_window(menu_frame_t *frame);
static float menu_get_cursor_target_width(void);

static void menu_list_refresh_targets(void);
static void menu_list_begin_enter(void);
static void menu_list_begin_exit(void);

static bool menu_list_move(int8_t direction);
static bool menu_list_update(uint32_t deltaTimeMs);
static bool menu_list_animation_finished(void);

static void menu_render_list(void);
static bool menu_go_back(void);

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
    },
    {
        .label = "SCROLL TEST 1",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "SCROLL TEST 2",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "SCROLL TEST 3",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
    },
    {
        .label = "SCROLL TEST 4",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_open_notice
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

/****************************************/



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
    //不直接显示最终列表，从重叠状态播放展开动画
    menu_list_begin_enter();
}

/* 获得菜单项最终需要显示的文字。 */
static const char *menu_get_item_label(const menu_item_t *item)
{
    if (item->getLabel != NULL){
        return item->getLabel();
    }

    return item->label;
}
// 返回当前列表字体的实际像素高度，使用已有的两种字体small和large的高度值
static uint8_t menu_get_font_height(void){
    if (APP_Settings_Get()->font == APP_FONT_SMALL)
    {
        return 10U;
    }

    return 13U;
}
//保证当前选中项目处于可见窗口内
static void menu_list_normalize_window(menu_frame_t *frame){
    uint8_t count;
    uint8_t capacity;
    uint8_t maximumFirstIndex;

    if ((frame == NULL) || (frame->page == NULL)){
        return;
    }
    count = frame->page->itemCount;
    // 16行像素高，一次显示4行
    capacity = (uint8_t)(UI_DISPLAY_HEIGHT / MENU_LIST_LINE_HEIGHT);
    if ((count == 0U) || (capacity == 0U)){
        frame->selectedIndex = 0U;
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = 0U;
        return;
    }
    if (frame->selectedIndex >= count){
        frame->selectedIndex = 0U;
    }
    //菜单项不超过4项不需要滚动，菜单始终按照自然顺序显示
    if (count <= capacity){
        frame->firstVisibleIndex = 0U;
        frame->cursorRow = frame->selectedIndex;
        return;
    }
    maximumFirstIndex = (uint8_t)(count - capacity); 
    if (frame->firstVisibleIndex > maximumFirstIndex){
        frame->firstVisibleIndex = maximumFirstIndex;
    } 
    //选中项位于当前窗口上方，将其移动到第一行
    if (frame->selectedIndex < frame->firstVisibleIndex){
        frame->firstVisibleIndex = frame->selectedIndex;
    }
    //选中项位于当前窗口下方，将其移动到最后一行
    else if (frame->selectedIndex >= frame->firstVisibleIndex + capacity){
        frame->firstVisibleIndex = frame->selectedIndex - capacity + 1U;
    }
    frame->cursorRow = (uint8_t)(frame->selectedIndex - frame->firstVisibleIndex);

}
//计算当前选中菜单项所需的光标宽度，光标右侧不进入5像素宽的滚动条区域
static float menu_get_cursor_target_width(void){
    menu_frame_t *frame = menu_current_frame();
    u8g2_t *u8g2 = UI_Display_GetContext();

    const char *label;
    uint16_t width;
    uint16_t maximumWidth;

    if((frame->page == NULL) || (frame->page->itemCount == 0U) || (frame->selectedIndex >= frame->page->itemCount)){
        return 0.0f;
    }
    label = menu_get_item_label(&frame->page->items[frame->selectedIndex]);
    if(label == NULL){
        return 0.0f;
    }
    menu_apply_font(u8g2);
    //文字左右各留4像素空白
    width = u8g2_GetStrWidth(u8g2, label) + MENU_LIST_TEXT_X * 2U;
    maximumWidth = UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH;
    if (width > maximumWidth){
        width = maximumWidth;
    }
    return (float)width;
}
//根据当前菜单选择更新全部列表动画目标：只修改目标值，不改变当前值
static void menu_list_refresh_targets(void){
    menu_frame_t *frame = menu_current_frame();

    uint8_t count;
    if ((frame == NULL) || (frame->page == NULL)){
        return;
    }
    count = frame->page->itemCount;
    menu_list_normalize_window(frame);

    s_menuAnimation.lineSpacingTarget = (float)MENU_LIST_LINE_HEIGHT;
    s_menuAnimation.contentTargetY = -(float)frame->firstVisibleIndex * (float)MENU_LIST_LINE_HEIGHT;
    s_menuAnimation.cursorTargetY = (float)frame->cursorRow * (float)MENU_LIST_LINE_HEIGHT;
    s_menuAnimation.cursorTargetWidth = menu_get_cursor_target_width();
    //滚动条在第一项时高度为0，在最后一项时高度为64
    if (count <= 1U){
        s_menuAnimation.barTargetY = (float)UI_DISPLAY_HEIGHT;
    }
    else{
        s_menuAnimation.barTargetY = (float)frame->selectedIndex * (float)UI_DISPLAY_HEIGHT / (float)(count - 1U);
    }
}
//开始列表进入动画，所有菜单文字首先重叠在当前光标行，随后行间距从0逐渐展开到16
static void menu_list_begin_enter(void){
    menu_list_refresh_targets();
    s_menuAnimation.phase = MENU_LIST_PHASE_ENTERING;
    s_menuAnimation.lineSpacing = 0.0f;
    //当行间距为0时，所有项目都位于同一纵坐标，随着行间距展开，项目分别移动到各自行位置
    s_menuAnimation.contentY = s_menuAnimation.cursorTargetY;
    s_menuAnimation.cursorY = s_menuAnimation.cursorTargetY;
    //光标宽度从0展开到文字宽度
    s_menuAnimation.cursorWidth = 0.0f;
    s_menuAnimation.barY = 0.0f;    //滚动条从上方逐渐展开
}
//列表退出动画，列表行逐渐收拢到当前选中行，光标和滚动条同时缩回
static void menu_list_begin_exit(void){
    if (s_menuAnimation.phase == MENU_LIST_PHASE_EXITING){
        return;
    }
    menu_list_refresh_targets();

    s_menuAnimation.phase = MENU_LIST_PHASE_EXITING;
    s_menuAnimation.lineSpacingTarget = 0.0f;
    //行间距变为0后，所有项目会重叠在光标行
    s_menuAnimation.contentTargetY = s_menuAnimation.cursorTargetY;

    s_menuAnimation.cursorTargetWidth = 0.0f;
    s_menuAnimation.barTargetY = 0.0f;
}
//判断所有列表动画参数是否达到目标值
static bool menu_list_animation_finished(void){
// UI_Animation_Approach会在接近目标时直接将当前值设置为目标值，这里可以直接比较
    return
        (s_menuAnimation.lineSpacing == s_menuAnimation.lineSpacingTarget) &&

        (s_menuAnimation.contentY == s_menuAnimation.contentTargetY) &&

        (s_menuAnimation.cursorY == s_menuAnimation.cursorTargetY) &&

        (s_menuAnimation.cursorWidth == s_menuAnimation.cursorTargetWidth) &&

        (s_menuAnimation.barY == s_menuAnimation.barTargetY);
}
//处理列表的上下选择
static bool menu_list_move(int8_t direction){
    menu_frame_t *frame = menu_current_frame();
    uint8_t count;

    if((frame == NULL) || (frame->page == NULL)){
        return false;
    }
    count = frame->page->itemCount;
    if (count == 0U || direction == 0){
        return false;
    }
    //列表展开或退出期间禁止移动，避免页面状态和动画目标发生冲突
    if (s_menuAnimation.phase != MENU_LIST_PHASE_ACTIVE){
        return false;
    }
    if(direction < 0){
        frame->selectedIndex = (uint8_t)((frame->selectedIndex + count - 1U) % count);
    }
    else{
        frame->selectedIndex = (uint8_t)((frame->selectedIndex + 1U) % count);
    }
    menu_list_refresh_targets();
    return true;
}
//更新列表动画
static bool menu_list_update(uint32_t deltaTimeMs){
    bool changed = false;

    changed |= UI_Animation_Approach(&s_menuAnimation.lineSpacing, s_menuAnimation.lineSpacingTarget,
        deltaTimeMs, MENU_LIST_ENTRY_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_menuAnimation.contentY, s_menuAnimation.contentTargetY,
        deltaTimeMs, MENU_LIST_SCROLL_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_menuAnimation.cursorY, s_menuAnimation.cursorTargetY,
        deltaTimeMs, MENU_LIST_CURSOR_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_menuAnimation.cursorWidth, s_menuAnimation.cursorTargetWidth,
        deltaTimeMs, MENU_LIST_CURSOR_WIDTH_RESPONSE_MS);

    changed |= UI_Animation_Approach(&s_menuAnimation.barY, s_menuAnimation.barTargetY,
        deltaTimeMs, MENU_LIST_BAR_RESPONSE_MS);

    if (!menu_list_animation_finished()){
        return changed;
    }

    if (s_menuAnimation.phase == MENU_LIST_PHASE_ENTERING){
        //列表已完全展开，现在可以响应按键
        s_menuAnimation.phase = MENU_LIST_PHASE_ACTIVE;
    }
    else if (s_menuAnimation.phase == MENU_LIST_PHASE_EXITING){
        //退出动画完成后才切换页面，避免按下返回键后画面立即跳到主菜单
        if (s_menuDepth > 0U){
            s_menuDepth--;
        }

        if (s_menuDepth == 0U){
            menu_tile_init();
        }
        else{
            /*
             * 为以后支持三级菜单保留。
             * 返回父列表时重新播放列表展开动画。
             */
            menu_list_begin_enter();
        }
        changed = true;
    }
    return changed;
}
//绘制列表页面
static void menu_render_list(void){
    menu_frame_t *frame = menu_current_frame();
    u8g2_t *u8g2 = UI_Display_GetContext();

    const app_settings_t *settings = APP_Settings_Get();
    uint8_t count;
    uint8_t fontHeight;
    uint8_t textOffsetY;

    uint16_t barHeight;
    uint16_t cursorWidth;

    int16_t cursorY;

    UI_Display_Clear();
    menu_apply_font(u8g2); 
    
    if ((frame == NULL) || (frame->page == NULL)){
        return;
    }

    count = frame->page->itemCount;

    if (count == 0U){
        return;
    }
    fontHeight = menu_get_font_height();

    if (fontHeight >= MENU_LIST_LINE_HEIGHT){
        textOffsetY = 0U;
    }
    else{//让文字在16像素行高中垂直居中
        textOffsetY = (uint8_t)((MENU_LIST_LINE_HEIGHT - fontHeight) / 2U);
    }   
    u8g2_SetDrawColor(u8g2, 1U);
    //绘制右侧滚动进度条,两条横线表示顶部和底部，中间竖线表示滚动范围，实心矩形表示当前进度。
    u8g2_DrawHLine(u8g2, UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH,
        0U, MENU_LIST_BAR_WIDTH);

    u8g2_DrawHLine(u8g2, UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH,
        UI_DISPLAY_HEIGHT - 1U, MENU_LIST_BAR_WIDTH);

    u8g2_DrawVLine(u8g2, UI_DISPLAY_WIDTH - ((MENU_LIST_BAR_WIDTH + 1U) / 2U),
        0U, UI_DISPLAY_HEIGHT); 
    
    if (s_menuAnimation.barY <= 0.0f){
        barHeight = 0U;
    }
    else{
        barHeight = (uint16_t)(s_menuAnimation.barY + 0.5f);
    }

    if (barHeight > UI_DISPLAY_HEIGHT){
        barHeight = UI_DISPLAY_HEIGHT;
    }

    if (barHeight > 0U){
        u8g2_DrawBox(u8g2, UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH,
            0U, MENU_LIST_BAR_WIDTH, (u8g2_uint_t)barHeight);
    }

    /*
     * 绘制全部菜单文字。
     * 超出屏幕的项目会被跳过，
     * 屏幕边缘处仍然依靠U8g2完成最终裁剪。
     */
    for (uint8_t index = 0U; index < count; index++){
        const char *label;
        float animatedRowY;
        int16_t rowY;
        int16_t textY;

        animatedRowY = (float)index * s_menuAnimation.lineSpacing + s_menuAnimation.contentY;

        rowY = menu_float_to_int16(animatedRowY);

        if ((rowY >= (int16_t)UI_DISPLAY_HEIGHT) || ((int16_t)(rowY + (int16_t)MENU_LIST_LINE_HEIGHT) <= 0)){
            continue;
        }

        label = menu_get_item_label(&frame->page->items[index]);

        if (label == NULL){
            label = "";
        }
        textY = (int16_t)(rowY + (int16_t)textOffsetY);
        u8g2_DrawStr(u8g2, MENU_LIST_TEXT_X, (u8g2_uint_t)textY, label);
    } 
    /*
     * 文字全部绘制完成以后再绘制光标。
     * XOR圆角矩形会同时反转文字和背景。
     */
    cursorY = menu_float_to_int16(s_menuAnimation.cursorY);

    if (s_menuAnimation.cursorWidth <= 0.0f){
        cursorWidth = 0U;
    }
    else{
        cursorWidth = (uint16_t)(s_menuAnimation.cursorWidth + 0.5f);
    }

    if (cursorWidth > (UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH)){
        cursorWidth = UI_DISPLAY_WIDTH - MENU_LIST_BAR_WIDTH;
    }

    if (cursorWidth == 0U){
        return;
    }

    if (settings->cursorStyle == APP_CURSOR_INVERSE){
        //圆角反色光标
        u8g2_SetDrawColor(u8g2, 2U);
        u8g2_DrawRBox(u8g2, 0U, (u8g2_uint_t)cursorY, (u8g2_uint_t)cursorWidth, MENU_LIST_LINE_HEIGHT, MENU_LIST_CURSOR_RADIUS);
        u8g2_SetDrawColor(u8g2, 1U);
    }
    else{//保留原工程的边框模式设置
        u8g2_DrawRFrame(u8g2, 0U, (u8g2_uint_t)cursorY,
            (u8g2_uint_t)cursorWidth, MENU_LIST_LINE_HEIGHT, MENU_LIST_CURSOR_RADIUS);
    }    
}
//返回上一级页面，只启动退出动画
static bool menu_go_back(void){
    if (s_menuDepth == 0U){
        return false;
    }

    if (s_menuAnimation.phase == MENU_LIST_PHASE_EXITING){
        return false;
    }
    menu_list_begin_exit();
    return true;
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



/* 执行当前选中的菜单项。 */
static void menu_activate_selected_item(void)
{
    menu_frame_t *frame = menu_current_frame();
    const menu_item_t *item;

    if ((frame->page == NULL) || (frame->page->itemCount == 0U)){
        return;
    }

    item = &frame->page->items[frame->selectedIndex];

    if (item->childPage != NULL){
        menu_enter_page(item->childPage);
    }
    else if (item->action != NULL){
        item->action();
        // FONT KEYSOUND等设置改变后，动态文字和光标宽度可能变化，只更新目标值
        if((s_currentView == MENU_VIEW_LIST) && (s_menuDepth > 0U)){
            menu_list_refresh_targets();
        }
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
    // menu_reset_animation();
    //一级菜单动画初始化，列表动画等进入Settings时初始化
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
                // menu_reset_animation();//提示页面返回时重置动画
                menu_list_begin_enter();    //从列表打开的提示页面返回时，重新播放列表展开动画
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
            // menu_move_up();
            // return true;
            return menu_list_move(-1);

        case MENU_KEY_DOWN:
            if(s_menuDepth ==0U){
                return menu_tile_move(1);   //按键2选择下一个图标
            }
            // menu_move_down();
            // return true;
            return menu_list_move(1);

        case MENU_KEY_OK:
            if((s_menuDepth > 0U) && (s_menuAnimation.phase != MENU_LIST_PHASE_ACTIVE)){
                return false;
            }
            menu_activate_selected_item();
            return true;

        case MENU_KEY_BACK:
            return menu_go_back();

        default:
            return false;
    }
}

bool Menu_Update(uint32_t deltaTimeMs){
    // bool changed = false;
    //提示页面没有菜单滚动动画。
    if (s_currentView != MENU_VIEW_LIST){
        return false;
    }
    //根页面对应M_MAIN，使用一级菜单动画
    if(s_menuDepth == 0U){
        return menu_tile_update(deltaTimeMs);
    }
    // |=是按位或赋值运算符，表示如果任何一个动画状态发生了变化，changed就会被置为true。
    // changed |= UI_Animation_Approach(&s_menuAnimation.currentY, s_menuAnimation.currentTargetY, deltaTimeMs, MENU_SCROLL_RESPONSE_MS);
    // changed |= UI_Animation_Approach(&s_menuAnimation.cursorY, s_menuAnimation.cursorTargetY, deltaTimeMs, MENU_CURSOR_RESPONSE_MS);
    // changed |= UI_Animation_Approach(&s_menuAnimation.cursorWidth, s_menuAnimation.cursorTargetWidth, deltaTimeMs, MENU_CURSOR_WIDTH_RESPONSE_MS);

    return menu_list_update(deltaTimeMs);    
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
