#include "ui_waveform.h"

#define UI_WAVEFORM_POINT_COUNT       92U
#define UI_WAVEFORM_UPDATE_PERIOD_MS  50U

#define UI_WAVEFORM_LEFT              34U
#define UI_WAVEFORM_TOP               3U
#define UI_WAVEFORM_WIDTH             92U
#define UI_WAVEFORM_HEIGHT            40U

static ui_waveform_sample_function_t s_sampleFunction;
static uint8_t s_points[UI_WAVEFORM_POINT_COUNT];

static uint32_t s_elapsedMs;
static uint16_t s_lastMillivolts;
static bool s_hasSignalSource;

static uint8_t ui_waveform_map_voltage(uint16_t millivolts){
    uint32_t value;
    if(millivolts > 3300U){
        millivolts = 3300U;
    }
    value = ((uint32_t)millivolts * (UI_WAVEFORM_HEIGHT - 3U)) / 3300U;

    return (uint8_t)(UI_WAVEFORM_TOP + UI_WAVEFORM_HEIGHT - 2U - value);
}

static void ui_waveform_format_voltage(char *text, uint16_t millivolts)
{
    text[0] = (char)('0' + ((millivolts / 1000U) % 10U));
    text[1] = '.';
    text[2] = (char)('0' + ((millivolts / 100U) % 10U));
    text[3] = (char)('0' + ((millivolts / 10U) % 10U));
    text[4] = (char)('0' + (millivolts % 10U));
    text[5] = 'V';
    text[6] = '\0';
}

void UI_Waveform_Init(ui_waveform_sample_function_t sampleFunction)
{
    s_sampleFunction = sampleFunction;
    UI_Waveform_Reset();
}

void UI_Waveform_Reset(void)
{
    uint8_t i;

    for (i = 0U; i < UI_WAVEFORM_POINT_COUNT; i++)
    {
        s_points[i] =
            UI_WAVEFORM_TOP +
            (UI_WAVEFORM_HEIGHT / 2U);
    }

    s_elapsedMs = 0U;
    s_lastMillivolts = 0U;
    s_hasSignalSource = false;
}

bool UI_Waveform_Update(uint32_t deltaTimeMs)
{
    uint16_t millivolts;
    uint8_t i;

    s_elapsedMs += deltaTimeMs;

    if (s_elapsedMs < UI_WAVEFORM_UPDATE_PERIOD_MS)
    {
        return false;
    }

    s_elapsedMs -= UI_WAVEFORM_UPDATE_PERIOD_MS;

    if ((s_sampleFunction == NULL) ||
        !s_sampleFunction(&millivolts))
    {
        s_hasSignalSource = false;
        return false;
    }

    s_hasSignalSource = true;
    s_lastMillivolts = millivolts;

    for (i = 0U; i < (UI_WAVEFORM_POINT_COUNT - 1U); i++)
    {
        s_points[i] = s_points[i + 1U];
    }

    s_points[UI_WAVEFORM_POINT_COUNT - 1U] =
        ui_waveform_map_voltage(millivolts);

    return true;
}

bool UI_Waveform_HasSignalSource(void)
{
    return s_hasSignalSource;
}

void UI_Waveform_Draw(u8g2_t *u8g2)
{
    uint8_t i;
    char voltageText[7];

    if (u8g2 == NULL)
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1U);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tf);

    u8g2_DrawStr(u8g2, 0, 8, "VOLT");
    u8g2_DrawStr(u8g2, 0, 63, "B4 BACK");

    u8g2_DrawRFrame(
        u8g2,
        UI_WAVEFORM_LEFT,
        UI_WAVEFORM_TOP,
        UI_WAVEFORM_WIDTH,
        UI_WAVEFORM_HEIGHT,
        2U);

    if (!s_hasSignalSource)
    {
        u8g2_DrawStr(u8g2, 48, 21, "COMING");
        u8g2_DrawStr(u8g2, 51, 31, "SOON");
        u8g2_DrawStr(u8g2, 45, 53, "ADC PORT");
        return;
    }

    for (i = 1U; i < UI_WAVEFORM_POINT_COUNT; i++)
    {
        u8g2_DrawLine(
            u8g2,
            (uint8_t)(UI_WAVEFORM_LEFT + i - 1U),
            s_points[i - 1U],
            (uint8_t)(UI_WAVEFORM_LEFT + i),
            s_points[i]);
    }

    ui_waveform_format_voltage(voltageText, s_lastMillivolts);
    u8g2_DrawStr(u8g2, 72, 55, voltageText);
}
