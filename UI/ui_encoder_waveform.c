#include "UI/ui_encoder_waveform.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ENCODER_HISTORY_COUNT 256U

typedef struct
{
    float rawRpm;
    float filteredRpm;
    uint32_t sequence;
    bool valid;
} encoder_wave_sample_t;

typedef struct
{
    encoder_wave_sample_t samples[ENCODER_HISTORY_COUNT];

    uint32_t filterTimeMs;
    uint16_t writeIndex;
    uint16_t count;

    bool active;
    bool dirty;
    uint32_t redrawElapsedMs;

    /* 两路曲线共用对称量程，默认 -120～120 RPM。 */
    float rangeRpm;

    uint8_t frame[UI_ENCODER_WAVE_FRAME_BYTES];
} encoder_wave_state_t;

static encoder_wave_state_t s_speed;

//在256x64页格式缓冲中画一个像素
static void encoder_draw_point(int16_t x, int16_t y){
    uint16_t index;
    uint8_t bit;

    if((x < 0) || (x >=(int16_t)UI_ENCODER_WAVE_WIDTH) || (y < 0) || (y >= (int16_t)UI_ENCODER_WAVE_HEIGHT)){
        return;
    }
    index = (uint16_t)(y / 8) * UI_ENCODER_WAVE_WIDTH + (uint16_t)x;
    bit = (uint8_t)(y % 8);
    s_speed.frame[index] |= (1 << bit);
}
// Bresenham直线，坐标会自动裁剪
static void encoder_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1){
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

        encoder_draw_point(x0, y0);

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

static int16_t encoder_map_y(float rpm)
{
    if (rpm > s_speed.rangeRpm)
        rpm = s_speed.rangeRpm;
    else if (rpm < -s_speed.rangeRpm)
        rpm = -s_speed.rangeRpm;

    return (int16_t)(
        (s_speed.rangeRpm - rpm) *
        (float)(UI_ENCODER_WAVE_HEIGHT - 1U) /
        (2.0f * s_speed.rangeRpm) + 0.5f);
}

static void encoder_render(void)
{
    uint16_t oldest;
    uint16_t startX;

    bool previousValid = false;
    uint32_t previousSequence = 0U;
    int16_t previousX = 0;
    int16_t previousY = 0;

    memset(s_speed.frame, 0, sizeof(s_speed.frame));

    /* 零速度参考线。 */
    for (uint16_t x = 0U; x < UI_ENCODER_WAVE_WIDTH; x += 8U)
    {
        encoder_draw_point((int16_t)x, encoder_map_y(0.0f));
    }

    oldest = (uint16_t)((s_speed.writeIndex + ENCODER_HISTORY_COUNT - s_speed.count) % ENCODER_HISTORY_COUNT);

    startX = UI_ENCODER_WAVE_WIDTH - s_speed.count;

    for (uint16_t i = 0U; i < s_speed.count; i++)
    {
        uint16_t index =
            (uint16_t)((oldest + i) % ENCODER_HISTORY_COUNT);

        const encoder_wave_sample_t *sample = &s_speed.samples[index];

        int16_t x = (int16_t)(startX + i);

        if (!sample->valid)
        {
            previousValid = false;
            continue;
        }

        /* 原始速度：只画点，不连接相邻样本。 */
        /* 每隔一列画点，让平稳段也呈现点状。 */
        if ((x & 1) == 0)
        {
            encoder_draw_point(x, encoder_map_y(sample->rawRpm));
        }

        /* 滤波速度：用正常连线函数连接。 */
        int16_t y = encoder_map_y(sample->filteredRpm);

        if (previousValid &&
            (uint32_t)(sample->sequence - previousSequence) == 1U)
        {
            encoder_draw_line(previousX, previousY, x, y);
        }
        else
        {
            encoder_draw_point(x, y);
        }

        previousX = x;
        previousY = y;
        previousSequence = sample->sequence;
        previousValid = true;
    }
}

void UI_EncoderWave_Init(void)
{
    memset(&s_speed, 0, sizeof(s_speed));
    s_speed.rangeRpm = 120.0f;
}

void UI_EncoderWave_Enter(void)
{
    s_speed.active = true;
    s_speed.dirty = true;
    s_speed.redrawElapsedMs = 40U;
}

void UI_EncoderWave_Leave(void)
{
    s_speed.active = false;
}

void UI_EncoderWave_AddSample(float rawRpm, float filteredRpm, uint32_t sequence, bool valid){
    encoder_wave_sample_t *sample = &s_speed.samples[s_speed.writeIndex];

    sample->rawRpm = rawRpm;
    sample->filteredRpm = filteredRpm;
    sample->sequence = sequence;
    sample->valid = valid;

    s_speed.writeIndex = (uint16_t)((s_speed.writeIndex + 1U) % ENCODER_HISTORY_COUNT);

    if (s_speed.count < ENCODER_HISTORY_COUNT){
        s_speed.count++;
    }

    s_speed.dirty = true;
}

bool UI_EncoderWave_Update(uint32_t deltaTimeMs)
{
    if (!s_speed.active)
    {
        return false;
    }

    if (s_speed.redrawElapsedMs < 40U)
    {
        s_speed.redrawElapsedMs += deltaTimeMs;
    }

    if (s_speed.dirty && s_speed.redrawElapsedMs >= 40U)
    {
        s_speed.redrawElapsedMs = 0U;
        return true;
    }

    return false;
}

void UI_EncoderWave_Scale(bool zoomIn)
{
    s_speed.rangeRpm *= zoomIn ? 0.5f : 2.0f;

    if (s_speed.rangeRpm < 15.0f)
        s_speed.rangeRpm = 15.0f;
    else if (s_speed.rangeRpm > 1920.0f)
        s_speed.rangeRpm = 1920.0f;

    s_speed.dirty = true;
}

bool UI_EncoderWave_PrepareFrame(const uint8_t **frameBuffer)
{
    if ((frameBuffer == NULL) ||
        !s_speed.active ||
        !s_speed.dirty)
    {
        return false;
    }

    encoder_render();

    *frameBuffer = s_speed.frame;
    s_speed.dirty = false;

    return true;
}

static void encoder_format_rpm(char *text, size_t size, float rpm)
{
    long tenths = (long)(
        rpm * 10.0f + ((rpm >= 0.0f) ? 0.5f : -0.5f));

    long magnitude = (tenths < 0) ? -tenths : tenths;

    (void)snprintf(
        text, size, "%s%ld.%ld",
        (tenths < 0) ? "-" : "",
        magnitude / 10,
        magnitude % 10);
}
//滤波时间显示接口
void UI_EncoderWave_SetFilterTimeMs(uint32_t filterTimeMs){
    if (s_speed.filterTimeMs != filterTimeMs){
        s_speed.filterTimeMs = filterTimeMs;
        s_speed.dirty = true;
    }
}
//1号屏幕的数值显示界面，整数格式化
void UI_EncoderWave_DrawControl(u8g2_t *u8g2)
{
    char value[24];
    char line[40];

    if (u8g2 == NULL){
        return;
    }

    u8g2_SetDrawColor(u8g2, 1U);
    u8g2_SetFontMode(u8g2, 1U);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_SetFontPosTop(u8g2);

    u8g2_DrawStr(u8g2, 0U, 0U, "ENCODER RPM");
    u8g2_DrawStr(u8g2, 0U, 12U, "DOT:RAW LINE:LPF");

    if (s_speed.count != 0U)
    {
        uint16_t index = (uint16_t)(
            (s_speed.writeIndex + ENCODER_HISTORY_COUNT - 1U) %
            ENCODER_HISTORY_COUNT);

        const encoder_wave_sample_t *sample = &s_speed.samples[index];

        if (sample->valid)
        {
            encoder_format_rpm(value, sizeof(value), sample->rawRpm);
            (void)snprintf(line, sizeof(line), "RAW: %s", value);
            u8g2_DrawStr(u8g2, 0U, 24U, line);

            encoder_format_rpm(
                value, sizeof(value), sample->filteredRpm);
            (void)snprintf(line, sizeof(line), "LPF: %s", value);
            u8g2_DrawStr(u8g2, 0U, 36U, line);
        }
        else
        {
            u8g2_DrawStr(u8g2, 0U, 24U, "WAIT / READ ERR");
        }
    }

    (void)snprintf(
        line,
        sizeof(line),
        "Tf=%lu +/- %ld RPM",
        (unsigned long)s_speed.filterTimeMs,
        (long)s_speed.rangeRpm);

    u8g2_DrawStr(u8g2, 0U, 48U, line);
}