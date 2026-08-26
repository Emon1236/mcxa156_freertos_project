/*
使用U8G2接口，
使用按毫秒计算的非阻塞动画，
使用4按键输入
增加确认和取消区别
背景虚化没添加
*/

#include "UI/ui_window.h"

#include "UI/ui_animation.h"
#include "UI/ui_display_port.h"

#include <stddef.h>
//弹窗布局参数沿用WouoUI版本
#define UI_WINDOW_WIDTH                 102U
#define UI_WINDOW_HEIGHT                32U
#define UI_WINDOW_RADIUS                2U

#define UI_WINDOW_BAR_WIDTH             92U
#define UI_WINDOW_BAR_HEIGHT            7U
#define UI_WINDOW_BAR_RADIUS            1U

//弹窗水平居中 (128 - 102) / 2 = 13
#define UI_WINDOW_LEFT ((UI_DISPLAY_WIDTH - UI_WINDOW_WIDTH) / 2U)
//弹窗垂直居中 (64 - 32) / 2 = 16
#define UI_WINDOW_TARGET_Y  ((UI_DISPLAY_HEIGHT - UI_WINDOW_HEIGHT) / 2U)
//弹窗初始位置位于屏幕上方，比弹窗高度多2像素，保证完全离开显示区域
#define UI_WINDOW_HIDDEN_Y              -34.0f

// #define UI_WINDOW_ANIMATION_RESPONSE_MS 90U
// #define UI_WINDOW_BAR_RESPONSE_MS       90U
//标题数组最大长度，包括结尾的'\0'.
#define UI_WINDOW_TITLE_CAPACITY        20U

typedef enum{
    UI_WINDOW_PHASE_OPENING = 0u,
    UI_WINDOW_PHASE_ACTIVE,
    UI_WINDOW_PHASE_CLOSING
} ui_window_phase_t;

typedef struct{
    bool visible;
    ui_window_phase_t phase;

    char title[UI_WINDOW_TITLE_CAPACITY];

    uint8_t value;
    uint8_t originalValue;
    uint8_t minimumValue;
    uint8_t maximumValue;
    uint8_t step;  
    // 数值变化回调
    ui_window_value_callback_t valueChanged;

    float y;
    float targetY;

    float bar;
    float barTarget;
}ui_window_state_t;
static ui_window_state_t s_window;
//窗口和进度条共用一个动画响应时间
static uint16_t s_windowAnimationResponseMs = 90U;

//把float绘图转换为int16_t，进行四舍五入
static int16_t ui_window_float_to_int16(float value){
    if (value >= 0.0f){
        return (int16_t)(value + 0.5f);
    }
    else{
        return (int16_t)(value - 0.5f);
    }
}

//将uint8_t转换为十进制字符串，不引入sprintf，避免增加代码体积
//缓冲区至少需要4个字节，255加结尾'\0'
static void ui_window_uint8_to_string(uint8_t value, char text[4]){
    if (value >= 100U){
        text[0] = (char)('0' + value / 100U);
        text[1] = (char)('0' + (value / 10U) % 10U);
        text[2] = (char)('0' + value % 10U);
        text[3] = '\0';
    }
    else if (value >= 10U){
        text[0] = (char)('0' + value / 10U);
        text[1] = (char)('0' + value % 10U);
        text[2] = '\0';
        text[3] = '\0';
    }
    else{
        text[0] = (char)('0' + value);
        text[1] = '\0';
        text[2] = '\0';
        text[3] = '\0';
    }
}
//根据当前数值计算进度条内部像素填充宽度
static float ui_window_calculate_bar_target(void){
    // 外框宽度为92，左右各留2像素
    float range;
    float position;

    if (s_window.maximumValue <= s_window.minimumValue){
        return 0.0f;
    }
    range = (float)(s_window.maximumValue - s_window.minimumValue);
    position = (float)(s_window.value - s_window.minimumValue);

    return position / range * (float)(UI_WINDOW_BAR_WIDTH - 4U);
}
//调用具体功能的实时应用回调
static void ui_window_apply_value(void){
    if(s_window.valueChanged != NULL){
        s_window.valueChanged(s_window.value);
    }
}
//启动弹窗关闭动画
static void ui_window_begin_close(void){
    s_window.phase = UI_WINDOW_PHASE_CLOSING;
    s_window.targetY = UI_WINDOW_HIDDEN_Y;
    // 关闭时让进度条同时收缩
    s_window.barTarget = 0.0f;
}
//
bool UI_Window_OpenValue(const char *title, uint8_t initialValue, uint8_t minimumValue,
                    uint8_t maximumValue, uint8_t step, ui_window_value_callback_t valueChanged){
    uint8_t index;

    if (s_window.visible){
        return false;
    }
    if((title == NULL) || (minimumValue > maximumValue) || (step == 0U)){
        return false;
    }
    if (initialValue < minimumValue){
        initialValue = minimumValue;
    }
    else if (initialValue > maximumValue){
        initialValue = maximumValue;
    }
    //手动复制标题，保证结尾'\0'，避免strcpy造成标题数组越界
    index = 0U;
    while ((index < (UI_WINDOW_TITLE_CAPACITY - 1U)) && (title[index] != '\0')){
        s_window.title[index] = title[index];
        index++;
    }
    s_window.title[index] = '\0';
    //AI建议清理标题数组剩余空间，有利于调试观察结构体内容
    index++;
    while(index < UI_WINDOW_TITLE_CAPACITY){
        s_window.title[index] = '\0';
        index++;
    }
    s_window.value = initialValue;
    s_window.originalValue = initialValue;

    s_window.minimumValue = minimumValue;
    s_window.maximumValue = maximumValue;
    s_window.step = step;

    s_window.valueChanged = valueChanged;

    s_window.visible = true;
    s_window.phase = UI_WINDOW_PHASE_OPENING;

    /*
     * 弹窗从屏幕上方滑入中央。
     */
    s_window.y = UI_WINDOW_HIDDEN_Y;
    s_window.targetY = UI_WINDOW_TARGET_Y;

    /*
     * 进度条从0逐渐增长到当前值。
     */
    s_window.bar = 0.0f;
    s_window.barTarget = ui_window_calculate_bar_target();

    return true;
}

bool UI_Window_IsVisible(void){
    return s_window.visible;
}

bool UI_Window_HandleInput(ui_window_input_t input){
    uint8_t newValue;
    //弹窗未打开或正在关闭时不处理输入
    if ((!s_window.visible) || (s_window.phase != UI_WINDOW_PHASE_ACTIVE)){
        return false;
    }
    //
    newValue = s_window.value;

    switch (input){
        case UI_WINDOW_INPUT_DECREASE://当输入为减少时，判断当前值是否大于最小值，如果大于最小值，则减去步长
            if (newValue > s_window.minimumValue){
                uint8_t distance = (uint8_t)(newValue - s_window.minimumValue);
                if (distance <= s_window.step){//如果减去步长后小于最小值，则将当前值设置为最小值
                    newValue = s_window.minimumValue;
                }
                else{
                    newValue = (uint8_t)(newValue - s_window.step);
                }
            }
            break;

        case UI_WINDOW_INPUT_INCREASE:
            if (newValue < s_window.maximumValue){
                uint8_t distance = (uint8_t)(s_window.maximumValue - newValue);

                if (distance <= s_window.step){
                    newValue = s_window.maximumValue;
                }
                else{
                    newValue = (uint8_t)(newValue + s_window.step);
                }
            }
            break;

        case UI_WINDOW_INPUT_CONFIRM:
            /*
             * 数值已经通过回调实时生效。
             * 确认只需要关闭弹窗。
             */
            ui_window_begin_close();
            return true;

        case UI_WINDOW_INPUT_CANCEL:
            /*
             * 取消时恢复打开弹窗前的值，
             * 并再次调用回调恢复具体功能。
             */
            s_window.value = s_window.originalValue;

            ui_window_apply_value();
            ui_window_begin_close();
            return true;

        default:
            return false;
    }

    if (newValue == s_window.value){
        //已经到达最大值或最小值，当前输入没有产生变化
        return false;
    }
    s_window.value = newValue;
    /*
     * 数值变化立即作用到具体功能。
     */
    ui_window_apply_value();
    /*
     * 更新进度条目标宽度，当前宽度会平滑靠近目标。
     */
    s_window.barTarget = ui_window_calculate_bar_target();

    return true;    
}

bool UI_Window_Update(uint32_t deltaTimeMs){
    bool changed = false;

    if (!s_window.visible){
        return false;
    }

    changed |= UI_Animation_Approach(&s_window.y, s_window.targetY,
        deltaTimeMs, s_windowAnimationResponseMs);

    changed |= UI_Animation_Approach(&s_window.bar, s_window.barTarget,
        deltaTimeMs, s_windowAnimationResponseMs);

    if ((s_window.phase == UI_WINDOW_PHASE_OPENING) && (s_window.y == s_window.targetY)){
        //弹窗已到达中央，现在允许处理按键
        s_window.phase = UI_WINDOW_PHASE_ACTIVE;
    }
    else if ((s_window.phase == UI_WINDOW_PHASE_CLOSING) && (s_window.y == s_window.targetY)){
        //弹窗完全离开屏幕后才结束显示
        s_window.visible = false;
    }
    return changed;
}

void UI_Window_Draw(u8g2_t *u8g2){
    int16_t windowY;
    int16_t titleY;
    int16_t valueY;

    uint16_t innerBarWidth;
    uint16_t valueWidth;
    uint16_t valueX;

    char valueText[4];

    if ((!s_window.visible) || (u8g2 == NULL)){
        return;
    }

    windowY = ui_window_float_to_int16(s_window.y);
    //WouoUI弹窗字体
    u8g2_SetFont(u8g2, u8g2_font_HelvetiPixel_tr);

    u8g2_SetFontPosBaseline(u8g2);
    u8g2_SetFontMode(u8g2, 1U);

    //先用黑色圆角矩形覆盖背景列表，再绘制白色圆角边框
    u8g2_SetDrawColor(u8g2, 0U);

    u8g2_DrawRBox(u8g2, UI_WINDOW_LEFT, (u8g2_uint_t)windowY, UI_WINDOW_WIDTH,
        UI_WINDOW_HEIGHT, UI_WINDOW_RADIUS);

    u8g2_SetDrawColor(u8g2, 1U);

    u8g2_DrawRFrame(u8g2, UI_WINDOW_LEFT, (u8g2_uint_t)windowY, UI_WINDOW_WIDTH,
        UI_WINDOW_HEIGHT, UI_WINDOW_RADIUS);
    //绘制进度条外框
    u8g2_DrawRFrame(u8g2, UI_WINDOW_LEFT + 5U, (u8g2_uint_t)( windowY + 20),
        UI_WINDOW_BAR_WIDTH, UI_WINDOW_BAR_HEIGHT, UI_WINDOW_BAR_RADIUS);

    if (s_window.bar <= 0.0f){
        innerBarWidth = 0U;
    }
    else{
        innerBarWidth = (uint16_t)(s_window.bar + 0.5f);
    }

    if (innerBarWidth > (UI_WINDOW_BAR_WIDTH - 4U)){
        innerBarWidth = UI_WINDOW_BAR_WIDTH - 4U;
    }
    //进度条内部上下左右各留2像素
    if (innerBarWidth > 0U){
        u8g2_DrawBox(u8g2, UI_WINDOW_LEFT + 7U, (u8g2_uint_t)(windowY + 22),
            (u8g2_uint_t)innerBarWidth, UI_WINDOW_BAR_HEIGHT - 4U);
    }

    titleY = (int16_t)(windowY + 14);
    valueY = titleY;

    u8g2_DrawStr(u8g2, UI_WINDOW_LEFT + 5U, (u8g2_uint_t)titleY, s_window.title);

    //将数值转换成字符串，并在弹窗右侧对齐
    ui_window_uint8_to_string(s_window.value, valueText);

    valueWidth = u8g2_GetStrWidth(u8g2, valueText);

    valueX = UI_WINDOW_LEFT + UI_WINDOW_WIDTH - 5U - valueWidth;

    u8g2_DrawStr(u8g2, (u8g2_uint_t)valueX, (u8g2_uint_t)valueY, valueText);
    //恢复正常实线绘制颜色，避免影响后续UI绘制
    u8g2_SetDrawColor(u8g2, 1U);
}

void UI_Window_SetAnimationResponseMs(uint16_t responseMs){
    if (responseMs == 0U){
        responseMs = 1U;
    }
    s_windowAnimationResponseMs = responseMs;
}