#include "ui_waveform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_WAVEFORM_SENSOR_MIN_Q4       (-40 * 16)  //温度传感器原始物理量的最小量程
#define UI_WAVEFORM_SENSOR_MAX_Q4       (125 * 16)  //温度传感器原始物理量的最大量程

#define UI_WAVEFORM_MIN_SPAN_Q4         (4 * 16)    //最小显示跨度
//异常值规避，超过两次的10°以上跳变才会接受
#define UI_WAVEFORM_JUMP_LIMIT_Q4       (10 * 16)   //跳变限制阈值
#define UI_WAVEFORM_JUMP_CONFIRM_Q4     (1 * 16)    //跳变确认阈值

#define UI_WAVEFORM_ENTRY_TIME_MS 350U   //350MS进入显示的时间

#define UI_WAVEFORM_SHRINK_HOLD_MS  5000U   //自动量程缩小前稳定等待时间

#define UI_WAVEFORM_EXPAND_RESPONSE_MS 150U //扩大量程快一些
#define UI_WAVEFORM_SHRINK_RESPONSE_MS 800U //缩小量程慢一些

typedef struct{
    int16_t samples[UI_WAVEFORM_SAMPLE_CAPACITY];    //最近256个有效温度
    uint16_t writeIndex;    //下一次写入位置
    uint16_t validCount;    //有效样本数量

    bool hasCurrentTemperature;
    bool sensorHealthy;
    int16_t currentTemperatureQ4;
    //大幅度跳变确认状态，单次异常值不会写入历史，也不会改变自动量程
    bool pendingJumpValid;
    int16_t pendingJumpQ4;

    uint32_t errorCount;

    ui_waveform_scale_mode_t scaleMode;

    int32_t scaleMinQ4;
    int32_t scaleMaxQ4;
    int32_t scaleTargetMinQ4;
    int32_t scaleTargetMaxQ4;

    uint32_t shrinkStableMs;

    bool active;
    //entryOffsetY从-64变成0
    int16_t entryOffsetY;
    uint32_t entryElapsedMs;

    bool pairFrameDirty;
    uint8_t pairFrame[UI_WAVEFORM_FRAME_BYTES];
} ui_waveform_state_t;

static ui_waveform_state_t s_wave;

//返回有符号整数绝对值
static int32_t ui_waveform_abs32(int32_t value){
    return (value < 0) ? -value : value;
}
//对负数进行向下取整
static int32_t ui_waveform_floor_to_step(int32_t value, int32_t step){
    int32_t quotient;
    int32_t remainder;
    quotient = value / step;
    remainder = value % step;
    if (remainder < 0) {
        quotient --;
    }
    return quotient * step;
}
//
static bool ui_waveform_approach_int32(int32_t *current, int32_t target, uint32_t deltaTimeMs, uint32_t responseMs){
    int32_t difference;
    int32_t step;
    //
    if (current == NULL){
        return false;
    }
    //已经达到目标值时必须返回false，否则Menu_Update会认为动画一直在变化，导致三块OLED持续整屏刷新
    if (*current == target){
        return false;
    }
    //没有经过时间时不更新动画
    if (deltaTimeMs == 0U){
        return false;
    }
    if (responseMs == 0U){
        *current = target;
        return true;
    }
    difference = target - *current;
    step = (int32_t)((int64_t)difference * deltaTimeMs / responseMs);
    //整数除法结果为0时，至少移动一个单位防止动画永远到达不了目标值
    if(step == 0){
        step = (difference > 0) ? 1: -1;
    }
    if (ui_waveform_abs32(step) >= ui_waveform_abs32(difference)){
        *current = target;
    }else{
        *current += step;
    }
    return true;
}
//在256x64页格式缓冲中画一个像素
static void ui_waveform_draw_point(int16_t x, int16_t y){
    uint16_t index;
    uint8_t bit;

    if((x < 0) || (x >=(int16_t)UI_WAVEFORM_FRAME_WIDTH) || (y < 0) || (y >= (int16_t)UI_WAVEFORM_FRAME_HEIGHT)){
        return;
    }
    index = (uint16_t)(y / 8) * UI_WAVEFORM_FRAME_WIDTH + (uint16_t)x;
    bit = (uint8_t)(y % 8);
    s_wave.pairFrame[index] |= (1 << bit);
}
// Bresenham直线，坐标会自动裁剪
static void ui_waveform_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1){
    int16_t dx;
    int16_t dy;
    int16_t sx;
    int16_t sy;
    int16_t error;
    
    dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);    
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    error = dx - dy;
    for (;;){
        int16_t doubleError;

        ui_waveform_draw_point(x0, y0);

        if ((x0 == x1) && (y0 == y1)){
            break;
        }

        doubleError = (int16_t)(2 * error);

        if (doubleError > -dy){
            error = (int16_t)(error - dy);
            x0 = (int16_t)(x0 + sx);
        }

        if (doubleError < dx){
            error = (int16_t)(error + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}
// 返回从最旧样本开始的环形缓冲索引
static uint16_t ui_waveform_get_oldest_index(void){
    return (uint16_t)(
        (s_wave.writeIndex +
         UI_WAVEFORM_SAMPLE_CAPACITY -
         s_wave.validCount) %
        UI_WAVEFORM_SAMPLE_CAPACITY);
}
//统计当前所有可见样本的最大最小值
static bool ui_waveform_get_data_range(int16_t *minimumQ4, int16_t *maximumQ4){
    uint16_t oldestIndex;
    uint16_t index;
    uint16_t i;
    int16_t minimum;
    int16_t maximum;

    if ((minimumQ4 == NULL) || (maximumQ4 == NULL) || (s_wave.validCount == 0U)){
        return false;
    }
    oldestIndex = ui_waveform_get_oldest_index();

    minimum = s_wave.samples[oldestIndex];
    maximum = minimum;
    // 256个样本全部正在显示，不能只统计最近的一部分，
    for (i = 1U; i < s_wave.validCount; i++){
        index = (uint16_t)((oldestIndex + i) % UI_WAVEFORM_SAMPLE_CAPACITY);

        if (s_wave.samples[index] < minimum){
            minimum = s_wave.samples[index];
        }

        if (s_wave.samples[index] > maximum){
            maximum = s_wave.samples[index];
        }
    }

    *minimumQ4 = minimum;
    *maximumQ4 = maximum;

    return true;
}
//根据所有可见数据计算目标量程
static void ui_waveform_calculate_desired_range(int32_t *desiredMinQ4, int32_t *desiredMaxQ4){
    static const int16_t tickCandidatesQ4[] ={
        8,      /* 0.5°C */
        16,     /* 1°C */
        32,     /* 2°C */
        80,     /* 5°C */
        160,    /* 10°C */
        320,    /* 20°C */
        800     /* 50°C */
    };
    int16_t dataMinQ4;
    int16_t dataMaxQ4;
    int32_t dataSpanQ4;
    int32_t marginQ4;
    int32_t requiredMinQ4;
    int32_t requiredMaxQ4;
    int32_t requiredSpanQ4;
    int32_t selectedMinQ4;
    int32_t selectedMaxQ4;
    int32_t selectedSpanQ4;
    uint8_t i;


    if (!ui_waveform_get_data_range(&dataMinQ4, &dataMaxQ4)){
        *desiredMinQ4 = 24 * 16;
        *desiredMaxQ4 = 28 * 16;
        return;
    }
    dataSpanQ4 = (int32_t)(dataMaxQ4 - dataMinQ4);
    //span/8位于上下两侧，总留白约为原数据跨度的25%
    marginQ4 = dataSpanQ4 / 8;
    //数据完全相同时，仍保留0.25°基础边距
    if (marginQ4 < 4){
        marginQ4 = 4;
    }
    requiredMinQ4 = (int32_t)dataMinQ4 - marginQ4;
    requiredMaxQ4 = (int32_t)dataMaxQ4 + marginQ4;
    requiredSpanQ4 = requiredMaxQ4 - requiredMinQ4;

    if (requiredSpanQ4 < UI_WAVEFORM_MIN_SPAN_Q4){
        int32_t centerQ4 = ((int32_t)dataMinQ4 + dataMaxQ4) / 2;
        requiredSpanQ4 = UI_WAVEFORM_MIN_SPAN_Q4;
        requiredMinQ4 = centerQ4 - requiredSpanQ4 / 2;
        requiredMaxQ4 = requiredMinQ4 + requiredSpanQ4;
    }
    selectedMinQ4 = requiredMinQ4;
    selectedMaxQ4 = requiredMaxQ4;
    selectedSpanQ4 = requiredSpanQ4;

    for (i = 0U;
        i < (uint8_t)(
             sizeof(tickCandidatesQ4) /
             sizeof(tickCandidatesQ4[0]));i++){
            int32_t tickQ4 = tickCandidatesQ4[i];
            int32_t spanQ4 = tickQ4 * 4;
            int32_t lowerQ4; 
    
        if (spanQ4 < requiredSpanQ4){
            continue;
        }
        lowerQ4 = ui_waveform_floor_to_step(requiredMinQ4, tickQ4);
        if ((lowerQ4 + spanQ4) >= requiredMaxQ4){
            selectedMinQ4 = lowerQ4;
            selectedMaxQ4 = lowerQ4 + spanQ4;
            selectedSpanQ4 = spanQ4;
            break;        
        }       
    }
    if (selectedSpanQ4 >
        (UI_WAVEFORM_SENSOR_MAX_Q4 -
         UI_WAVEFORM_SENSOR_MIN_Q4))
    {
        selectedMinQ4 = UI_WAVEFORM_SENSOR_MIN_Q4;
        selectedMaxQ4 = UI_WAVEFORM_SENSOR_MAX_Q4;
    }
    else
    {
        if (selectedMinQ4 < UI_WAVEFORM_SENSOR_MIN_Q4)
        {
            selectedMinQ4 = UI_WAVEFORM_SENSOR_MIN_Q4;
            selectedMaxQ4 =
                selectedMinQ4 + selectedSpanQ4;
        }

        if (selectedMaxQ4 > UI_WAVEFORM_SENSOR_MAX_Q4)
        {
            selectedMaxQ4 = UI_WAVEFORM_SENSOR_MAX_Q4;
            selectedMinQ4 =
                selectedMaxQ4 - selectedSpanQ4;
        }
    }

    if (selectedSpanQ4 >
        (UI_WAVEFORM_SENSOR_MAX_Q4 -
         UI_WAVEFORM_SENSOR_MIN_Q4))
    {
        selectedMinQ4 = UI_WAVEFORM_SENSOR_MIN_Q4;
        selectedMaxQ4 = UI_WAVEFORM_SENSOR_MAX_Q4;
    }
    else
    {
        if (selectedMinQ4 < UI_WAVEFORM_SENSOR_MIN_Q4)
        {
            selectedMinQ4 = UI_WAVEFORM_SENSOR_MIN_Q4;
            selectedMaxQ4 =
                selectedMinQ4 + selectedSpanQ4;
        }

        if (selectedMaxQ4 > UI_WAVEFORM_SENSOR_MAX_Q4)
        {
            selectedMaxQ4 = UI_WAVEFORM_SENSOR_MAX_Q4;
            selectedMinQ4 =
                selectedMaxQ4 - selectedSpanQ4;
        }
    }

    *desiredMinQ4 = selectedMinQ4;
    *desiredMaxQ4 = selectedMaxQ4;
}

static uint8_t ui_waveform_map_temperature_to_y(int16_t temperatureQ4){
    int32_t rangeQ4;
    int32_t relativeQ4;
    int32_t y;

    rangeQ4 = s_wave.scaleMaxQ4 - s_wave.scaleMinQ4;

    if (rangeQ4 <= 0){
        return UI_WAVEFORM_FRAME_HEIGHT / 2U;
    }

    relativeQ4 = (int32_t)temperatureQ4 - s_wave.scaleMinQ4;

    y = (UI_WAVEFORM_FRAME_HEIGHT - 1) - relativeQ4 * (UI_WAVEFORM_FRAME_HEIGHT - 1) / rangeQ4;

    if (y < 0){
        y = 0;
    }
    else if (y >= UI_WAVEFORM_FRAME_HEIGHT){
        y = UI_WAVEFORM_FRAME_HEIGHT - 1;
    }

    return (uint8_t)y;
}

static void ui_waveform_render_pair_frame(void){
    uint16_t oldestIndex;
    uint16_t startX;
    uint16_t i;
    bool previousValid = false;
    int16_t previousX = 0;
    int16_t previousY = 0;

    memset(s_wave.pairFrame, 0, sizeof(s_wave.pairFrame));
    if (!s_wave.active){
        return;
    }
    for (uint8_t grid = 0U; grid < 4U; grid++){
        int16_t sourceY;
        int16_t drawY;

        sourceY = (int16_t)(grid * (UI_WAVEFORM_FRAME_HEIGHT - 1U) / 3U);

        drawY = (int16_t)(sourceY + s_wave.entryOffsetY);

        for (uint16_t x = 0U; x < UI_WAVEFORM_FRAME_WIDTH; x += 4U){
            ui_waveform_draw_point((int16_t)x, drawY);
        }
    }

    if (s_wave.validCount == 0U){
        return;
    }

    oldestIndex = ui_waveform_get_oldest_index();

    /*
     * 有效数据不足256个时靠右显示。
     *
     * 例如只有10个样本：
     * x=0～245为空；
     * x=246～255显示真实数据。
     */
    startX = UI_WAVEFORM_FRAME_WIDTH - s_wave.validCount;

    for (i = 0U; i < s_wave.validCount; i++){
        uint16_t sampleIndex;
        int16_t x;
        int16_t y;

        sampleIndex = (uint16_t)((oldestIndex + i) % UI_WAVEFORM_SAMPLE_CAPACITY);

        x = (int16_t)(startX + i);

        y = (int16_t)(ui_waveform_map_temperature_to_y(s_wave.samples[sampleIndex]) + s_wave.entryOffsetY);

        if (previousValid){
            ui_waveform_draw_line(previousX, previousY, x, y);
        }else{
            ui_waveform_draw_point(x, y);
        }

        previousX = x;
        previousY = y;
        previousValid = true;
    } 
}
// 将温度值格式化为字符串，单位为摄氏度，保留一位小数。
static void ui_waveform_format_temperature(int16_t temperatureQ4, char *text, size_t textSize){
    int32_t tenths;
    int32_t absoluteTenths;

    if (temperatureQ4 >= 0){
        tenths = ((int32_t)temperatureQ4 * 10 + 8) / 16;
    }
    else{
        tenths = -(((int32_t)(-temperatureQ4) * 10 + 8) / 16);
    }

    absoluteTenths = (tenths < 0) ? -tenths : tenths;

    (void)snprintf(text, textSize, "%s%ld.%ld C", (tenths < 0) ? "-" : "",
        (long)(absoluteTenths / 10), (long)(absoluteTenths % 10));
}

static void ui_waveform_format_scale(char *text, size_t textSize){
    int32_t spanQ4;
    int32_t tenths;

    spanQ4 = s_wave.scaleMaxQ4 - s_wave.scaleMinQ4;

    tenths = (spanQ4 * 10 + 8) / 16;

    (void)snprintf(text, textSize, "%s %ld.%ldC",
        (s_wave.scaleMode == UI_WAVEFORM_SCALE_AUTO) ? "AUTO" : "HOLD",
        (long)(tenths / 10), (long)(tenths % 10));
}

void UI_Waveform_Init(void){
    memset(&s_wave, 0, sizeof(s_wave));

    s_wave.sensorHealthy = false;
    s_wave.scaleMode = UI_WAVEFORM_SCALE_AUTO;

    s_wave.scaleMinQ4 = 24 * 16;
    s_wave.scaleMaxQ4 = 28 * 16;

    s_wave.scaleTargetMinQ4 = s_wave.scaleMinQ4;
    s_wave.scaleTargetMaxQ4 = s_wave.scaleMaxQ4;

    s_wave.entryOffsetY = -(int16_t)UI_WAVEFORM_FRAME_HEIGHT;
}

bool UI_Waveform_AddTemperatureSample(int16_t temperatureQ4, bool valid){
    bool previousHealth;
    previousHealth = s_wave.sensorHealthy;
    if (!valid){
        s_wave.sensorHealthy = false;
        s_wave.errorCount++;
        return s_wave.active && (previousHealth != s_wave.sensorHealthy);
    }
    if ((temperatureQ4 < UI_WAVEFORM_SENSOR_MIN_Q4) || (temperatureQ4 > UI_WAVEFORM_SENSOR_MAX_Q4)){
        s_wave.sensorHealthy = false;
        s_wave.errorCount++;
        return s_wave.active && (previousHealth != s_wave.sensorHealthy);
    }
    s_wave.sensorHealthy = true;
    //次异常跳变确认。第一次大幅跳变：只记录为pending，不写入历史。
    if (s_wave.hasCurrentTemperature && (ui_waveform_abs32((int32_t)temperatureQ4 - s_wave.currentTemperatureQ4) >  UI_WAVEFORM_JUMP_LIMIT_Q4)){
        if (s_wave.pendingJumpValid && (ui_waveform_abs32((int32_t)temperatureQ4 -s_wave.pendingJumpQ4) <= UI_WAVEFORM_JUMP_CONFIRM_Q4)){
            s_wave.pendingJumpValid = false;
        }else{
            s_wave.pendingJumpValid = true;
            s_wave.pendingJumpQ4 = temperatureQ4;
            return s_wave.active && (previousHealth != s_wave.sensorHealthy);
        }
    }else{
        s_wave.pendingJumpValid = false;
    }
    s_wave.samples[s_wave.writeIndex] = temperatureQ4;
    s_wave.writeIndex = (uint16_t)((s_wave.writeIndex + 1U) % UI_WAVEFORM_SAMPLE_CAPACITY);
    if (s_wave.validCount < UI_WAVEFORM_SAMPLE_CAPACITY){
        s_wave.validCount++;
    }
    s_wave.currentTemperatureQ4 = temperatureQ4;
    s_wave.hasCurrentTemperature = true;
    if (s_wave.active){
        s_wave.pairFrameDirty = true;
        return true;        
    }
    return false;
}

void UI_Waveform_Enter(void){
    int32_t desiredMinQ4;
    int32_t desiredMaxQ4;

    ui_waveform_calculate_desired_range(&desiredMinQ4, &desiredMaxQ4);
    s_wave.scaleMode = UI_WAVEFORM_SCALE_AUTO;

    s_wave.scaleMinQ4 = desiredMinQ4;
    s_wave.scaleMaxQ4 = desiredMaxQ4;
    s_wave.scaleTargetMinQ4 = desiredMinQ4;
    s_wave.scaleTargetMaxQ4 = desiredMaxQ4;

    s_wave.shrinkStableMs = 0U;

    s_wave.active = true;
    s_wave.entryElapsedMs = 0U;
    s_wave.entryOffsetY = -(int16_t)UI_WAVEFORM_FRAME_HEIGHT;

    s_wave.pairFrameDirty = true;       
}

void UI_Waveform_Leave(void){
    s_wave.active = false;

    /*
     * PreparePairFrame会生成全零缓冲，
     * app_ui_task随后把它发送给2、3号屏。
     */
    s_wave.pairFrameDirty = true;
}
bool UI_Waveform_Update(uint32_t deltaTimeMs)
{
    bool changed = false;
    uint32_t responseMs;
    int32_t desiredMinQ4;
    int32_t desiredMaxQ4;

    if (!s_wave.active)
    {
        return false;
    }

    /* 推出动画。 */
    if (s_wave.entryElapsedMs <
        UI_WAVEFORM_ENTRY_TIME_MS)
    {
        int16_t newOffsetY;

        s_wave.entryElapsedMs += deltaTimeMs;

        if (s_wave.entryElapsedMs >
            UI_WAVEFORM_ENTRY_TIME_MS)
        {
            s_wave.entryElapsedMs =
                UI_WAVEFORM_ENTRY_TIME_MS;
        }

        newOffsetY =
            (int16_t)(
                -((int32_t)UI_WAVEFORM_FRAME_HEIGHT) +
                ((int32_t)UI_WAVEFORM_FRAME_HEIGHT *
                 s_wave.entryElapsedMs) /
                UI_WAVEFORM_ENTRY_TIME_MS);

        if (newOffsetY != s_wave.entryOffsetY)
        {
            s_wave.entryOffsetY = newOffsetY;
            changed = true;
        }
    }

    ui_waveform_calculate_desired_range(
        &desiredMinQ4,
        &desiredMaxQ4);

    if (s_wave.scaleMode == UI_WAVEFORM_SCALE_AUTO)
    {
        int32_t desiredSpanQ4 =
            desiredMaxQ4 - desiredMinQ4;

        int32_t targetSpanQ4 =
            s_wave.scaleTargetMaxQ4 -
            s_wave.scaleTargetMinQ4;

        /*
         * 数据逼近或越过当前目标边界时快速扩张。
         */
        if ((desiredMinQ4 <
             s_wave.scaleTargetMinQ4) ||
            (desiredMaxQ4 >
             s_wave.scaleTargetMaxQ4))
        {
            s_wave.scaleTargetMinQ4 = desiredMinQ4;
            s_wave.scaleTargetMaxQ4 = desiredMaxQ4;
            s_wave.shrinkStableMs = 0U;
        }
        /*
         * 数据只占当前量程60%以内时开始收缩计时。
         */
        else if ((desiredSpanQ4 * 100) <=
                 (targetSpanQ4 * 60))
        {
            s_wave.shrinkStableMs += deltaTimeMs;

            if (s_wave.shrinkStableMs >=
                UI_WAVEFORM_SHRINK_HOLD_MS)
            {
                s_wave.scaleTargetMinQ4 = desiredMinQ4;
                s_wave.scaleTargetMaxQ4 = desiredMaxQ4;
                s_wave.shrinkStableMs = 0U;
            }
        }
        else
        {
            s_wave.shrinkStableMs = 0U;
        }
    }

    if ((s_wave.scaleTargetMinQ4 <
         s_wave.scaleMinQ4) ||
        (s_wave.scaleTargetMaxQ4 >
         s_wave.scaleMaxQ4))
    {
        responseMs = UI_WAVEFORM_EXPAND_RESPONSE_MS;
    }
    else
    {
        responseMs = UI_WAVEFORM_SHRINK_RESPONSE_MS;
    }

    changed |= ui_waveform_approach_int32(
        &s_wave.scaleMinQ4,
        s_wave.scaleTargetMinQ4,
        deltaTimeMs,
        responseMs);

    changed |= ui_waveform_approach_int32(
        &s_wave.scaleMaxQ4,
        s_wave.scaleTargetMaxQ4,
        deltaTimeMs,
        responseMs);

    if (changed)
    {
        s_wave.pairFrameDirty = true;
    }

    return changed;
}
void UI_Waveform_ScaleIn(void)
{
    int32_t spanQ4;
    int32_t centerQ4;

    if (!s_wave.active)
    {
        return;
    }

    spanQ4 =
        s_wave.scaleTargetMaxQ4 -
        s_wave.scaleTargetMinQ4;

    spanQ4 /= 2;

    if (spanQ4 < UI_WAVEFORM_MIN_SPAN_Q4)
    {
        spanQ4 = UI_WAVEFORM_MIN_SPAN_Q4;
    }

    centerQ4 =
        (s_wave.scaleTargetMinQ4 +
         s_wave.scaleTargetMaxQ4) / 2;

    s_wave.scaleTargetMinQ4 =
        centerQ4 - spanQ4 / 2;

    s_wave.scaleTargetMaxQ4 =
        s_wave.scaleTargetMinQ4 + spanQ4;

    s_wave.scaleMode = UI_WAVEFORM_SCALE_HOLD;
    s_wave.pairFrameDirty = true;
}

void UI_Waveform_ScaleOut(void)
{
    int32_t spanQ4;
    int32_t centerQ4;
    int32_t physicalSpanQ4;

    if (!s_wave.active)
    {
        return;
    }

    physicalSpanQ4 =
        UI_WAVEFORM_SENSOR_MAX_Q4 -
        UI_WAVEFORM_SENSOR_MIN_Q4;

    spanQ4 =
        (s_wave.scaleTargetMaxQ4 -
         s_wave.scaleTargetMinQ4) * 2;

    if (spanQ4 > physicalSpanQ4)
    {
        spanQ4 = physicalSpanQ4;
    }

    centerQ4 =
        (s_wave.scaleTargetMinQ4 +
         s_wave.scaleTargetMaxQ4) / 2;

    s_wave.scaleTargetMinQ4 =
        centerQ4 - spanQ4 / 2;

    s_wave.scaleTargetMaxQ4 =
        s_wave.scaleTargetMinQ4 + spanQ4;

    if (s_wave.scaleTargetMinQ4 <
        UI_WAVEFORM_SENSOR_MIN_Q4)
    {
        s_wave.scaleTargetMinQ4 =
            UI_WAVEFORM_SENSOR_MIN_Q4;

        s_wave.scaleTargetMaxQ4 =
            s_wave.scaleTargetMinQ4 + spanQ4;
    }

    if (s_wave.scaleTargetMaxQ4 >
        UI_WAVEFORM_SENSOR_MAX_Q4)
    {
        s_wave.scaleTargetMaxQ4 =
            UI_WAVEFORM_SENSOR_MAX_Q4;

        s_wave.scaleTargetMinQ4 =
            s_wave.scaleTargetMaxQ4 - spanQ4;
    }

    s_wave.scaleMode = UI_WAVEFORM_SCALE_HOLD;
    s_wave.pairFrameDirty = true;
}

void UI_Waveform_ToggleAutoHold(void)
{
    if (!s_wave.active)
    {
        return;
    }

    if (s_wave.scaleMode == UI_WAVEFORM_SCALE_AUTO)
    {
        s_wave.scaleMode = UI_WAVEFORM_SCALE_HOLD;

        s_wave.scaleTargetMinQ4 = s_wave.scaleMinQ4;
        s_wave.scaleTargetMaxQ4 = s_wave.scaleMaxQ4;
    }
    else
    {
        s_wave.scaleMode = UI_WAVEFORM_SCALE_AUTO;

        ui_waveform_calculate_desired_range(
            &s_wave.scaleTargetMinQ4,
            &s_wave.scaleTargetMaxQ4);

        s_wave.shrinkStableMs = 0U;
    }

    s_wave.pairFrameDirty = true;
}
void UI_Waveform_DrawControl(u8g2_t *u8g2)
{
    char temperatureText[16];
    char scaleText[16];
    uint16_t textWidth;
    int16_t textX;

    if (u8g2 == NULL){
        return;
    }

    u8g2_SetDrawColor(u8g2, 1U);
    u8g2_SetFontMode(u8g2, 1U);
    u8g2_SetFontPosBaseline(u8g2);

    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(u8g2, 0U, 10U, "TEMP SENSOR");

    if (s_wave.hasCurrentTemperature)
    {
        ui_waveform_format_temperature(
            s_wave.currentTemperatureQ4,
            temperatureText,
            sizeof(temperatureText));
    }
    else
    {
        (void)snprintf(
            temperatureText,
            sizeof(temperatureText),
            "--.- C");
    }

    u8g2_SetFont(u8g2, u8g2_font_helvB18_tf);

    textWidth =
        u8g2_GetStrWidth(u8g2, temperatureText);

    textX =
        (textWidth < 128U)
            ? (int16_t)((128U - textWidth) / 2U)
            : 0;

    u8g2_DrawStr(
        u8g2,
        (u8g2_uint_t)textX,
        38U,
        temperatureText);

    ui_waveform_format_scale(
        scaleText,
        sizeof(scaleText));

    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);

    textWidth =
        u8g2_GetStrWidth(u8g2, scaleText);

    textX =
        (textWidth < 128U)
            ? (int16_t)((128U - textWidth) / 2U)
            : 0;

    u8g2_DrawStr(
        u8g2,
        (u8g2_uint_t)textX,
        58U,
        scaleText);

    if (!s_wave.sensorHealthy)
    {
        u8g2_DrawStr(u8g2, 90U, 10U, "ERR");
    }
    return;
}
//当2，3号屏幕需要刷新时，绘制最新的256x64帧，返回true时，frameBuffer指向待发送缓冲
bool UI_Waveform_PerpareFrame(const uint8_t **frameBuffer)
{
    if ((frameBuffer == NULL) || !s_wave.pairFrameDirty)
    {
        return false;
    }

    ui_waveform_render_pair_frame();

    *frameBuffer = s_wave.pairFrame;
    s_wave.pairFrameDirty = false;

    return true;
}

bool UI_Waveform_IsActive(void)
{
    return s_wave.active;
}