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
#include "UI/ui_window.h"
#include "UI/ui_effects.h"
#include "UI/ui_platform.h"
#include "UI/ui_sleep.h"
#include "UI/ui_waveform.h"
#include "UI/ui_encoder_waveform.h"

// #include "bsp/bsp_4oleds.h"
//#include "bsp/bsp_buzzer.h"

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
static bool s_pairWasActive;
//页面淡出完成后需要执行的操作
typedef enum{
    MENU_TRANSITION_NONE = 0U,
    MENU_TRANSITION_ENTER_PAGE,//进入一个子菜单
    MENU_TRANSITION_BACK,//从当前子菜单返回上一级
    MENU_TRANSITION_OPEN_NOTICE,//打开提示页面
    MENU_TRANSITION_OPEN_WAVEFORM,//打开波形页面
    MENU_TRANSITION_OPEN_ENCODER,//编码器页面
    MENU_TRANSITION_OPEN_SLEEP,//打开睡眠页面
    //从提示页或波形页回到原菜单，它不会修改s_menuDepth
    MENU_TRANSITION_RETURN_TO_MENU
} menu_transition_action_t;

typedef struct{
    bool active;
    /*
     * 淡出完成后先显示一次全黑/全白帧，
     * 下一次Update才真正切换页面。
     */
    bool finalFrameShown;
    menu_transition_action_t action;
    const menu_page_t *targetPage;
} menu_transition_state_t;
static menu_transition_state_t s_transition;
//当前提示页文字
static const char *s_noticeMessage = "COMING SOON";
//二选一弹窗的默认参数
static menu_foc_direction_t s_focDirection = MENU_FOC_DIRECTION_POSITIVE;


/* 静态函数声明区 */
static void menu_action_font(void);
static void menu_action_key_sound(void);
static void menu_action_cursor(void);
//static void menu_action_open_notice(void);

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
//static void menu_list_begin_exit(void);

static bool menu_list_move(int8_t direction);
static bool menu_list_update(uint32_t deltaTimeMs);
static bool menu_list_animation_finished(void);

static void menu_render_list(void);
static bool menu_go_back(void);

static void menu_tile_init(void);
static bool menu_tile_move(int8_t direction);
static bool menu_tile_update(uint32_t deltaTimeMs);
static void menu_render_tile(void);

static void menu_action_display_brightness(void);
static void menu_apply_display_brightness(uint8_t brightness);

/* 原有设置动作。 */
static void menu_action_display_brightness(void);
static void menu_apply_display_brightness(uint8_t brightness);
static void menu_action_font(void);
static void menu_action_key_sound(void);
static void menu_action_cursor(void);
/* 动画数值弹窗。 */
static void menu_action_tile_animation(void);
static void menu_action_list_animation(void);
static void menu_action_window_animation(void);
static void menu_action_fade_animation(void);

static void menu_apply_tile_animation(uint8_t value);
static void menu_apply_list_animation(uint8_t value);
static void menu_apply_window_animation(uint8_t value);
static void menu_apply_fade_animation(uint8_t value);

/* 设置开关。 */
static void menu_action_tile_unfold(void);
static void menu_action_list_unfold(void);
static void menu_action_tile_loop(void);
static void menu_action_list_loop(void);
static void menu_action_window_bokeh(void);
static void menu_action_dark_mode(void);

/* 功能页面。 */
static void menu_action_sleep(void);
//static void menu_action_voltage(void);
static void menu_action_hid(void);
static void menu_action_save_settings(void);
static void menu_action_coming_soon(void);

/* 动态菜单文字。 */
static const char *menu_get_font_label(void);
static const char *menu_get_key_sound_label(void);
static const char *menu_get_cursor_label(void);
static const char *menu_get_tile_unfold_label(void);
static const char *menu_get_list_unfold_label(void);
static const char *menu_get_tile_loop_label(void);
static const char *menu_get_list_loop_label(void);
static const char *menu_get_window_bokeh_label(void);
static const char *menu_get_dark_mode_label(void);

/* 页面切换。 */
static void menu_transition_begin(
    menu_transition_action_t action,
    const menu_page_t *targetPage);

static bool menu_transition_update(uint32_t deltaTimeMs);
static void menu_transition_commit(void);
static void menu_show_notice(const char *message);

/* 波形采样适配。 */
//static bool menu_read_voltage(uint16_t *millivolts);

static void menu_action_temperature(void);
static void menu_action_encoder(void);

static const menu_page_t s_editorPage;
static const menu_page_t s_settingsPage;
static const menu_page_t s_aboutPage;
static const menu_page_t s_wavePage;

/* FOC改变方向的弹窗菜单 */
static void menu_action_foc_openloop(void);
static void menu_apply_foc_direction(uint8_t value);

/* Editor列表：保留参考工程的功能入口。 */
static const menu_item_t s_editorItems[] =
{
    {
        .label = "FUNCTION 0",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 1",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 2",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 3",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 4",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 5",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 6",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 7",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 8",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 9",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "KNOB / HID",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_hid
    },
    {
        .label = "FUNCTION 11",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 12",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 13",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 14",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "FUNCTION 15",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    }
};

static const menu_page_t s_editorPage =
{
    .items = s_editorItems,
    .itemCount =
        (uint8_t)(sizeof(s_editorItems) / sizeof(s_editorItems[0]))
};

/* About页面只显示信息，不执行动作。 */
static const menu_item_t s_aboutItems[] =
{
    {
        .label = "[ ABOUT ]",
        .getLabel = NULL,
        .childPage = NULL,
        .action = NULL
    },
    {
        .label = "WOUOUI STYLE",
        .getLabel = NULL,
        .childPage = NULL,
        .action = NULL
    },
    {
        .label = "MCXA156",
        .getLabel = NULL,
        .childPage = NULL,
        .action = NULL
    },
    {
        .label = "U8G2 OLED",
        .getLabel = NULL,
        .childPage = NULL,
        .action = NULL
    }
};

static const menu_page_t s_aboutPage =
{
    .items = s_aboutItems,
    .itemCount =
        (uint8_t)(sizeof(s_aboutItems) / sizeof(s_aboutItems[0]))
};

/*
 * Settings合并了：
 * 1. 原工程的字体、按键音、光标；
 * 2. 参考工程中的动画和显示风格；
 * 3. EEPROM和HID的预留入口。
 */
static const menu_item_t s_settingsItems[] ={
    /* 最初的4个设置项。 */
    {
        .label = "OLED BRIGHTNESS", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_display_brightness
    },
    {
        .label = NULL, .getLabel = menu_get_font_label,
        .childPage = NULL, .action = menu_action_font
    },
    {
        .label = NULL, .getLabel = menu_get_key_sound_label,
        .childPage = NULL, .action = menu_action_key_sound
    },
    {
        .label = NULL, .getLabel = menu_get_cursor_label,
        .childPage = NULL, .action = menu_action_cursor
    },
    {
        .label = "TILE ANI", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_tile_animation
    },
    {
        .label = "LIST ANI", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_list_animation
    },
    {
        .label = "WINDOW ANI", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_window_animation
    },
    {
        .label = "FADE ANI", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_fade_animation
    },

    /* 参考UI中的开关设置。 */
    {
        .label = NULL, .getLabel = menu_get_tile_unfold_label,
        .childPage = NULL, .action = menu_action_tile_unfold
    },
    {
        .label = NULL, .getLabel = menu_get_list_unfold_label,
        .childPage = NULL, .action = menu_action_list_unfold
    },
    {
        .label = NULL,  .getLabel = menu_get_tile_loop_label,
        .childPage = NULL, .action = menu_action_tile_loop
    },
    {
        .label = NULL, .getLabel = menu_get_list_loop_label,
        .childPage = NULL, .action = menu_action_list_loop
    },
    {
        .label = NULL, .getLabel = menu_get_window_bokeh_label,
        .childPage = NULL, .action = menu_action_window_bokeh
    },
    {
        .label = NULL, .getLabel = menu_get_dark_mode_label,
        .childPage = NULL, .action = menu_action_dark_mode
    },
    /* 当前平台尚未提供对应功能。 */
    {
        .label = "SPOT ANI", .getLabel = NULL,
        .childPage = NULL, .action = menu_action_coming_soon
    },
    {
        .label = "TAG ANI",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "BTN SPT",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "BTN LPT",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "KNOB ROT DIR",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    },
    {
        .label = "USB HID",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_hid
    },
    {
        .label = "SAVE SETTINGS",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_save_settings
    },
    {
        .label = "ABOUT",
        .getLabel = NULL,
        .childPage = &s_aboutPage,
        .action = NULL
    }
};
static const menu_item_t s_waveItems[] ={
    {
        .label = "TEMP SENSOR",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_temperature
    },
    {
        .label = "ENCODER SENSOR",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_encoder
    },
    {
        .label = "VOLT",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_coming_soon
    } 
};
static const menu_page_t s_wavePage = {
    .items = s_waveItems,
    .itemCount = (uint8_t)(sizeof(s_waveItems) / sizeof(s_waveItems[0]))
};

static const menu_page_t s_settingsPage =
{
    .items = s_settingsItems,
    .itemCount =
        (uint8_t)(sizeof(s_settingsItems) / sizeof(s_settingsItems[0]))
};
static const menu_item_t s_focItems[] = {
    {
        .label = "openloop",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_foc_openloop 
    }
};

static const menu_page_t s_focPage = {
    .items = s_focItems,
    .itemCount = (uint8_t)(sizeof(s_focItems) / sizeof(s_focItems[0]))
};

/* 一级图形菜单，数量必须与g_uiMainIcons保持一致。 */
static const menu_item_t s_rootItems[] =
{
    {
        .label = "Sleep",
        .getLabel = NULL,
        .childPage = NULL,
        .action = menu_action_sleep
    },
    {
        .label = "Editor",
        .getLabel = NULL,
        .childPage = &s_editorPage,
        .action = NULL
    },
    {
        .label = "Wave",
        .getLabel = NULL,
        .childPage = &s_wavePage,
        .action = NULL
    },
    {
        .label = "FOC",
        .getLabel = NULL,
        .childPage = &s_focPage,
        .action = NULL
    },
    {
        .label = "Setting",
        .getLabel = NULL,
        .childPage = &s_settingsPage,
        .action = NULL
    }
};

static const menu_page_t s_rootPage =
{
    .items = s_rootItems,
    .itemCount =
        (uint8_t)(sizeof(s_rootItems) / sizeof(s_rootItems[0]))
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

/**************************************/
//淡出动画功能函数
static void menu_transition_begin(menu_transition_action_t action, const menu_page_t *targetPage){
    const app_settings_t *settings = APP_Settings_Get();

    if (s_transition.active){
        return;
    }

    s_transition.active = true;
    s_transition.finalFrameShown = false;
    s_transition.action = action;
    s_transition.targetPage = targetPage;

    /*
     * 暗色模式淡出到黑色。
     * 亮色模式淡出到白色。
     */
    UI_Effects_FadeBegin(settings->fadeStepMs, !settings->darkModeEnabled);
}

/* 淡出结束以后真正修改菜单状态。 */
static void menu_transition_commit(void){
    switch (s_transition.action){
        case MENU_TRANSITION_ENTER_PAGE:
            s_currentView = MENU_VIEW_LIST;
            menu_enter_page(s_transition.targetPage);
            break;

        case MENU_TRANSITION_BACK:
            if (s_menuDepth > 0U){
                s_menuDepth--;
            }

            s_currentView = MENU_VIEW_LIST;

            if (s_menuDepth == 0U){
                menu_tile_init();
            }
            else{
                menu_list_begin_enter();
            }
            break;

        case MENU_TRANSITION_OPEN_NOTICE:
            s_currentView = MENU_VIEW_NOTICE;
            break;

        case MENU_TRANSITION_OPEN_WAVEFORM:
            s_currentView = MENU_VIEW_WAVEFORM;
            // Enter只开始显示和推出动画
            UI_Waveform_Enter();
            break;

        case MENU_TRANSITION_OPEN_ENCODER:
            s_currentView = MENU_VIEW_ENCODER_WAVE;
            UI_EncoderWave_Enter();
            break;

        case MENU_TRANSITION_OPEN_SLEEP:
            s_currentView = MENU_VIEW_SLEEP;
            UI_Sleep_Enter(UI_Display_GetContext());
            break;

        case MENU_TRANSITION_RETURN_TO_MENU:
            s_currentView = MENU_VIEW_LIST;

            if (s_menuDepth == 0U){
                menu_tile_init();
            }
            else{
                menu_list_begin_enter();
            }
            break;

        case MENU_TRANSITION_NONE:
        default:
            break;
    }

    s_transition.active = false;
    s_transition.finalFrameShown = false;
    s_transition.action = MENU_TRANSITION_NONE;
    s_transition.targetPage = NULL;

    UI_Effects_FadeReset();
}

static bool menu_transition_update(uint32_t deltaTimeMs){
    bool changed;

    if (!s_transition.active){
        return false;
    }

    changed = UI_Effects_FadeUpdate(deltaTimeMs);

    if (!UI_Effects_FadeIsFinished()){
        return changed;
    }

    /*
     * step 4是全黑或全白。
     * 先要求任务把这帧发送到OLED，然后下一周期再切换页面。
     */
    if (!s_transition.finalFrameShown){
        s_transition.finalFrameShown = true;
        return true;
    }

    menu_transition_commit();
    return true;
}

static void menu_show_notice(const char *message){
    if (message == NULL){
        message = "COMING SOON";
    }

    s_noticeMessage = message;

    menu_transition_begin(
        MENU_TRANSITION_OPEN_NOTICE,
        NULL);
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
    //LIST UFD关闭时，列表直接处于最终位置,不播放行距从0展开到16的动画
    if (!APP_Settings_Get()->listUnfoldEnabled)
    {
        s_menuAnimation.lineSpacing =
            s_menuAnimation.lineSpacingTarget;

        s_menuAnimation.contentY =
            s_menuAnimation.contentTargetY;

        s_menuAnimation.cursorY =
            s_menuAnimation.cursorTargetY;

        s_menuAnimation.cursorWidth =
            s_menuAnimation.cursorTargetWidth;

        s_menuAnimation.barY =
            s_menuAnimation.barTargetY;

        s_menuAnimation.phase =
            MENU_LIST_PHASE_ACTIVE;
    }    
}
//列表退出动画，列表行逐渐收拢到当前选中行，光标和滚动条同时缩回
// static void menu_list_begin_exit(void){
//     if (s_menuAnimation.phase == MENU_LIST_PHASE_EXITING){
//         return;
//     }
//     menu_list_refresh_targets();

//     s_menuAnimation.phase = MENU_LIST_PHASE_EXITING;
//     //s_menuAnimation.lineSpacingTarget = 0.0f;
//     //行间距变为0后，所有项目会重叠在光标行
//     s_menuAnimation.contentTargetY = s_menuAnimation.cursorTargetY;

//     s_menuAnimation.cursorTargetWidth = 0.0f;
//     s_menuAnimation.barTargetY = 0.0f;
// }
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
    if (direction < 0){
        if (frame->selectedIndex == 0U){
            if (!APP_Settings_Get()->listLoopEnabled){
                return false;
            }
            frame->selectedIndex = (uint8_t)(count - 1U);
        }
        else{
            frame->selectedIndex--;
        }
    }
    else{
        if (frame->selectedIndex >= (uint8_t)(count - 1U)){
            if (!APP_Settings_Get()->listLoopEnabled){
                return false;
            }
            frame->selectedIndex = 0U;
        }
        else{
            frame->selectedIndex++;
        }
    }
    menu_list_refresh_targets();
    return true;
}
//更新列表动画
static bool menu_list_update(uint32_t deltaTimeMs){
    const app_settings_t *settings = APP_Settings_Get();
    uint32_t responseMs;
    bool changed = false;

    responseMs = settings->listAnimationMs;

    if(responseMs == 0U){
        responseMs = 1U;
    }
    changed |= UI_Animation_Approach(&s_menuAnimation.lineSpacing, s_menuAnimation.lineSpacingTarget,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_menuAnimation.contentY, s_menuAnimation.contentTargetY,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_menuAnimation.cursorY, s_menuAnimation.cursorTargetY,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_menuAnimation.cursorWidth, s_menuAnimation.cursorTargetWidth,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_menuAnimation.barY, s_menuAnimation.barTargetY,
        deltaTimeMs, responseMs);

    if (!menu_list_animation_finished()){
        return changed;
    }

    if (s_menuAnimation.phase == MENU_LIST_PHASE_ENTERING){
        //列表已完全展开，现在可以响应按键
        s_menuAnimation.phase = MENU_LIST_PHASE_ACTIVE;
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

    if (s_menuAnimation.phase != MENU_LIST_PHASE_ACTIVE){
        return false;
    }
    if (s_transition.active){
        return false;
    }
    menu_transition_begin(MENU_TRANSITION_BACK, NULL);    
    //menu_list_begin_exit();
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
    /*
    * TILE UFD关闭时，一级菜单直接显示最终状态。
    */
    if (!APP_Settings_Get()->tileUnfoldEnabled)
    {
        s_tileAnimation.iconSpacing = s_tileAnimation.iconSpacingTarget;

        s_tileAnimation.iconOffsetX = s_tileAnimation.iconOffsetTargetX;

        s_tileAnimation.iconY = s_tileAnimation.iconTargetY;

        s_tileAnimation.indicatorWidth = s_tileAnimation.indicatorTargetWidth;

        s_tileAnimation.titleY = s_tileAnimation.titleTargetY;

        s_tileAnimation.ready = true;
    }
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
    if (direction > 0)
    {
        if (frame->selectedIndex >= (uint8_t)(count - 1U))
        {
            if (!APP_Settings_Get()->tileLoopEnabled)
            {
                return false;
            }

            frame->selectedIndex = 0U;
        }
        else
        {
            frame->selectedIndex++;
        }
    }
    else
    {
        if (frame->selectedIndex == 0U)
        {
            if (!APP_Settings_Get()->tileLoopEnabled)
            {
                return false;
            }

            frame->selectedIndex = (uint8_t)(count - 1U);
        }
        else
        {
            frame->selectedIndex--;
        }
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
    const app_settings_t *settings = APP_Settings_Get();
    uint32_t responseMs;
    bool changed = false;
    responseMs = settings->tileAnimationMs;

    changed |= UI_Animation_Approach(&s_tileAnimation.iconSpacing, s_tileAnimation.iconSpacingTarget,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_tileAnimation.iconOffsetX, s_tileAnimation.iconOffsetTargetX,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_tileAnimation.iconY, s_tileAnimation.iconTargetY,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_tileAnimation.indicatorWidth, s_tileAnimation.indicatorTargetWidth,
        deltaTimeMs, responseMs);

    changed |= UI_Animation_Approach(&s_tileAnimation.titleY, s_tileAnimation.titleTargetY,
        deltaTimeMs, responseMs);

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
         * 当前M_MAIN菜单项和图标数量均为5。
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
        menu_transition_begin(MENU_TRANSITION_ENTER_PAGE, item->childPage);
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
    const char *message = s_noticeMessage;
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

// /* 根据当前设置播放按键音。 */
// static void menu_play_key_tone(void){
//     const app_settings_t *settings = APP_Settings_Get();

//     if (settings->keySoundEnabled){
//         BSP_Buzzer_PlayTone(MENU_KEY_TONE_HZ, MENU_KEY_TONE_MS);
//     }
// }

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
/**************************************/
// TILE, LIST, WINDOW, DARK MODE动态文字。
static const char *menu_get_tile_unfold_label(void)
{
    return APP_Settings_Get()->tileUnfoldEnabled
        ? "TILE UFD: ON"
        : "TILE UFD: OFF";
}

static const char *menu_get_list_unfold_label(void)
{
    return APP_Settings_Get()->listUnfoldEnabled
        ? "LIST UFD: ON"
        : "LIST UFD: OFF";
}

static const char *menu_get_tile_loop_label(void)
{
    return APP_Settings_Get()->tileLoopEnabled
        ? "TILE LOOP: ON"
        : "TILE LOOP: OFF";
}

static const char *menu_get_list_loop_label(void)
{
    return APP_Settings_Get()->listLoopEnabled
        ? "LIST LOOP: ON"
        : "LIST LOOP: OFF";
}

static const char *menu_get_window_bokeh_label(void)
{
    return APP_Settings_Get()->windowBokehEnabled
        ? "WIN BOKEH: ON"
        : "WIN BOKEH: OFF";
}

static const char *menu_get_dark_mode_label(void)
{
    return APP_Settings_Get()->darkModeEnabled
        ? "DARK MODE: ON"
        : "DARK MODE: OFF";
}

/***********************************************************************/
/* SETTINGS功能回调。 */
static void menu_apply_display_brightness(uint8_t brightness){
    APP_Settings_SetBrightness(brightness);
    u8g2_SetContrast(UI_Display_GetContext(), brightness);
}
//打开亮度调节弹窗
static void menu_action_display_brightness(void){
    const app_settings_t *settings = APP_Settings_Get();
    //最小值使用5而不是0，防止测试时把屏幕完全调黑后难以恢复
    (void)UI_Window_OpenValue("OLED BRI", settings->displayBrightness,
        10U, 255U, 5U, menu_apply_display_brightness);
}
static void menu_action_font(void){
    APP_Settings_NextFont();
}

static void menu_action_key_sound(void){
    APP_Settings_ToggleKeySound();
}

static void menu_action_cursor(void){
    APP_Settings_NextCursorStyle();
}

static void menu_apply_tile_animation(uint8_t value){
    APP_Settings_SetTileAnimationMs(value);
}

static void menu_apply_list_animation(uint8_t value){
    APP_Settings_SetListAnimationMs(value);
}

static void menu_apply_window_animation(uint8_t value){
    APP_Settings_SetWindowAnimationMs(value);

    /*
     * 如果给ui_window增加了运行时动画速度接口，在这里同步设置。
     */
    UI_Window_SetAnimationResponseMs(value);
}

static void menu_apply_fade_animation(uint8_t value){
    APP_Settings_SetFadeStepMs(value);
}

static void menu_action_tile_animation(void){
    const app_settings_t *settings = APP_Settings_Get();
    UI_Window_OpenValue("Tile Ani", settings->tileAnimationMs, 20U, 200U, 5U, menu_apply_tile_animation);
}

static void menu_action_list_animation(void){
    const app_settings_t *settings = APP_Settings_Get();
    UI_Window_OpenValue("List Ani", settings->listAnimationMs, 20U, 200U, 5U, menu_apply_list_animation);
}

static void menu_action_window_animation(void)
{
    const app_settings_t *settings = APP_Settings_Get();
    UI_Window_OpenValue("Window Ani", settings->windowAnimationMs,  20U, 200U, 5U, menu_apply_window_animation);
}

static void menu_action_fade_animation(void){
    const app_settings_t *settings = APP_Settings_Get();
    UI_Window_OpenValue("Fade Ani", settings->fadeStepMs, 10U, 100U, 5U, menu_apply_fade_animation);
}

static void menu_action_tile_unfold(void){
    APP_Settings_ToggleTileUnfold();
}

static void menu_action_list_unfold(void){
    APP_Settings_ToggleListUnfold();
}

static void menu_action_tile_loop(void){
    APP_Settings_ToggleTileLoop();
}

static void menu_action_list_loop(void){
    APP_Settings_ToggleListLoop();
}

static void menu_action_window_bokeh(void){
    APP_Settings_ToggleWindowBokeh();
}

static void menu_action_dark_mode(void){
    APP_Settings_ToggleDarkMode();
}

static void menu_action_hid(void){
        menu_show_notice("COMING SOON");
}

static void menu_action_save_settings(void){
    const app_settings_t *settings = APP_Settings_Get();
    if (UI_Platform_SettingsSave(settings, sizeof(app_settings_t)))
    {
        menu_show_notice("SAVED");
    }
    else{
        menu_show_notice("COMING SOON");
    }
}

static void menu_action_coming_soon(void){
    menu_show_notice("COMING SOON");
}

static void menu_action_sleep(void)
{
    menu_transition_begin(
        MENU_TRANSITION_OPEN_SLEEP,
        NULL);
}

// static void menu_action_voltage(void)
// {
//     menu_transition_begin(
//         MENU_TRANSITION_OPEN_WAVEFORM,
//         NULL);
// }
static void menu_action_temperature(void)
{
    menu_transition_begin(
        MENU_TRANSITION_OPEN_WAVEFORM,
        NULL);
}
static void menu_action_encoder(void){
    menu_transition_begin(
        MENU_TRANSITION_OPEN_ENCODER,
        NULL);
}

static void menu_apply_foc_direction(uint8_t value){
    s_focDirection = (value == 0U) ? MENU_FOC_DIRECTION_POSITIVE : MENU_FOC_DIRECTION_NEGATIVE;
}


static void menu_action_foc_openloop(void){
    uint8_t initialValue;
    initialValue = (s_focDirection == MENU_FOC_DIRECTION_NEGATIVE) ? 1U : 0U;

    (void)UI_Window_OpenChoice( "direction", initialValue, "+", "-", menu_apply_foc_direction);
}

menu_foc_direction_t Menu_GetFocDirection(void){
    return s_focDirection;
}
/* 所有尚未完成的一级菜单暂时调用这个函数。 */
// static void menu_action_open_notice(void){
//     s_currentView = MENU_VIEW_NOTICE;
// }

void Menu_Init(void){
    APP_Settings_Init();
    //默认亮度同步到OLED
    menu_apply_display_brightness(APP_Settings_Get()->displayBrightness);
    UI_Window_SetAnimationResponseMs(APP_Settings_Get()->windowAnimationMs);
    UI_Waveform_Init();
    UI_EncoderWave_Init();
    UI_Effects_FadeReset();
    s_pairWasActive = false;

    s_transition.active = false;
    s_transition.finalFrameShown = false;
    s_transition.action = MENU_TRANSITION_NONE;
    s_transition.targetPage = NULL;

    s_noticeMessage = "COMING SOON";

    s_menuDepth = 0U;
    s_currentView = MENU_VIEW_LIST;
    s_focDirection = MENU_FOC_DIRECTION_POSITIVE;

    s_menuStack[0].page = &s_rootPage;
    s_menuStack[0].selectedIndex = 0U;
    s_menuStack[0].firstVisibleIndex = 0U;
    s_menuStack[0].cursorRow = 0U;
    //新增动画初始化
    // menu_reset_animation();
    //一级菜单动画初始化，列表动画等进入Settings时初始化
    menu_tile_init();
}

//内部函数，只处理菜单动作，返回是否需要重绘
static bool menu_handle_key_action(menu_key_t key)
{
    // 优先处理弹窗
    if (UI_Window_IsVisible()){
    switch (key){
        case MENU_KEY_UP:
            return UI_Window_HandleInput(UI_WINDOW_INPUT_DECREASE);
        case MENU_KEY_DOWN:
            return UI_Window_HandleInput(UI_WINDOW_INPUT_INCREASE);
        case MENU_KEY_OK:
            return UI_Window_HandleInput(UI_WINDOW_INPUT_CONFIRM);
        case MENU_KEY_BACK:
            return UI_Window_HandleInput(UI_WINDOW_INPUT_CANCEL);
        default:
            return false;
        }
    }
    //提示页返回原来的菜单层级
    if (s_currentView == MENU_VIEW_NOTICE){
        if ((key == MENU_KEY_OK) || (key == MENU_KEY_BACK)){
            menu_transition_begin(MENU_TRANSITION_RETURN_TO_MENU, NULL);
            return true;
        }
        return false;
    }
    if (s_currentView == MENU_VIEW_ENCODER_WAVE){
        switch (key){
            case MENU_KEY_UP:
                UI_EncoderWave_Scale(true);
                return true;

            case MENU_KEY_DOWN:
                UI_EncoderWave_Scale(false);
                return true;

            case MENU_KEY_BACK:
                UI_EncoderWave_Leave();
                menu_transition_begin(MENU_TRANSITION_RETURN_TO_MENU, NULL);
                return true;

            default:
                return false;
        }
    }

    //温度波形页面的按键处理
    if (s_currentView == MENU_VIEW_WAVEFORM){
        switch (key){
            case MENU_KEY_UP:
                /*
                * B1缩小量程并进入HOLD。
                */
                UI_Waveform_ScaleIn();
                return true;

            case MENU_KEY_DOWN:
                /*
                * B2扩大量程并进入HOLD。
                */
                UI_Waveform_ScaleOut();
                return true;

            case MENU_KEY_OK:
                /*
                * B3切换AUTO/HOLD。
                * 1号屏不绘制按键说明，只显示模式。
                */
                UI_Waveform_ToggleAutoHold();
                return true;

            case MENU_KEY_BACK:
                /*
                * 先请求2、3号屏清空，
                * 再让1号屏淡出返回Wave二级菜单。
                */
                UI_Waveform_Leave();

                menu_transition_begin(
                    MENU_TRANSITION_RETURN_TO_MENU,
                    NULL);

                return true;

            default:
                return false;
        }        
    }

    //睡眠页面只处理返回和确认键，屏幕关闭时看不到淡出
    if (s_currentView == MENU_VIEW_SLEEP){
        if ((key == MENU_KEY_BACK) || (key == MENU_KEY_OK)){
            UI_Sleep_Wake(UI_Display_GetContext());
            s_currentView = MENU_VIEW_LIST;
            menu_tile_init();
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

menu_key_result_t Menu_HandleKey(menu_key_t key)
{
    menu_key_result_t result = {
        .needRedraw = false,
        .requestKeyFeedback = false
    };
    /* 页面过渡期间忽略输入，也不请求按键反馈。 */
    if (s_transition.active){
        return result;
    }
    /*
     * 必须在处理菜单动作之前读取设置：
     *
     * ON -> OFF：本次仍请求反馈。
     * OFF -> ON：本次不请求反馈，从下次开始。
     *
     *这里只生成结果，不调用 App、蜂鸣器或 RTOS。
     */
    result.requestKeyFeedback = APP_Settings_Get()->keySoundEnabled;
    result.needRedraw = menu_handle_key_action(key);

    return result;
}

bool Menu_Update(uint32_t deltaTimeMs){
    // bool changed = false;
    //背景列表保持原位置，不继续响应滚动动画
    if (UI_Window_IsVisible()){
        return UI_Window_Update(deltaTimeMs);
    }
    //页面淡出由菜单状态机统一更新
    if (s_transition.active)
    {
        return menu_transition_update(deltaTimeMs);
    }
    if (s_currentView == MENU_VIEW_WAVEFORM)
    {
        return UI_Waveform_Update(deltaTimeMs);
    }
    if (s_currentView == MENU_VIEW_ENCODER_WAVE)
    {
        return UI_EncoderWave_Update(deltaTimeMs);
    }

    if (s_currentView == MENU_VIEW_SLEEP)
    {
        return UI_Sleep_Update(
            UI_Display_GetContext(),
            deltaTimeMs);
    }

    if (s_currentView == MENU_VIEW_NOTICE)
    {
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

bool Menu_PreparePairFrame(const uint8_t **frameBuffer){
    static const uint8_t emptyFrame[UI_WAVEFORM_FRAME_BYTES] = {0U};

    if (frameBuffer == NULL){
        return false;
    }
    *frameBuffer = NULL;

    if (s_currentView == MENU_VIEW_ENCODER_WAVE){
        s_pairWasActive = true;
        return UI_EncoderWave_PrepareFrame(frameBuffer);
    }
    if (s_currentView == MENU_VIEW_WAVEFORM){
        s_pairWasActive = true;
        return UI_Waveform_PerpareFrame(frameBuffer);
    }
    if (s_pairWasActive){
        s_pairWasActive = false;
        *frameBuffer = emptyFrame;
        return true;
    }
    return false;
}

void Menu_Render(void){
    u8g2_t *u8g2 = UI_Display_GetContext();
    const app_settings_t *settings = APP_Settings_Get();
    UI_Display_Clear();
    switch (s_currentView)
    {
        case MENU_VIEW_NOTICE:
            menu_render_notice();
            break;

        case MENU_VIEW_WAVEFORM:
            UI_Waveform_DrawControl(u8g2);
            break;

        case MENU_VIEW_ENCODER_WAVE:
            UI_EncoderWave_DrawControl(u8g2);
            break;

        case MENU_VIEW_SLEEP:
            UI_Sleep_Draw(u8g2);
            break;

        case MENU_VIEW_LIST:
        default:
            if (s_menuDepth == 0U)
            {
                menu_render_tile();
            }
            else
            {
                menu_render_list();
            }
            break;
    }    
    //弹窗背景虚化必须位于：背景页面绘制完成之后、弹窗绘制之前
    if (UI_Window_IsVisible())
    {
        if (settings->windowBokehEnabled)
        {
            UI_Effects_ApplyBokeh(u8g2);
        }

        UI_Window_Draw(u8g2);
    } 
    //   当前页面默认黑底白字，XOR全屏得到白底黑字
    if (!settings->darkModeEnabled)
    {
        u8g2_SetDrawColor(u8g2, 2U);
        u8g2_DrawBox(
            u8g2,
            0U,
            0U,
            UI_DISPLAY_WIDTH,
            UI_DISPLAY_HEIGHT);

        u8g2_SetDrawColor(u8g2, 1U);
    }
    //淡出是帧缓冲的最后一步处理
    if (s_transition.active){
        UI_Effects_ApplyFade(u8g2);
    }
    // if (s_currentView == MENU_VIEW_NOTICE){
    //     menu_render_notice();
    // }
    // else if(s_menuDepth == 0U){
    //     menu_render_tile(); //一级根菜单用UI图标样式
    // }
    // else{
    //     //先绘制设置列表作为弹窗背景，render_list函数会清空缓冲区
    //     menu_render_list();
    //     if (UI_Window_IsVisible()){
    //         UI_Window_Draw(UI_Display_GetContext());
    //     }
    // }
    // /*
    //  * 所有绘图完成后只刷新一次，
    //  * 避免刷新过程中出现闪烁。
    //  */
    // BSP_4OLEDs_Refresh();
}
