#include "ui_sleep.h"

#define UI_SLEEP_FRAME_PERIOD_MS  250U
#define UI_SLEEP_POWER_OFF_MS     1800U

typedef struct{
    bool active;
    bool poweredOff;

    uint8_t frame;
    uint32_t totalTimeMs;
    uint32_t frameTimeMs;
} ui_sleep_state_t;

static ui_sleep_state_t s_sleep;

void UI_Sleep_Enter(u8g2_t *u8g2){
    if (u8g2 == NULL)
    {
        return;
    }

    u8g2_SetPowerSave(u8g2, 0U);

    s_sleep.active = true;
    s_sleep.poweredOff = false;
    s_sleep.frame = 0U;
    s_sleep.totalTimeMs = 0U;
    s_sleep.frameTimeMs = 0U;
}

bool UI_Sleep_Update(u8g2_t *u8g2, uint32_t deltaTimeMs){
    bool changed = false;

    if (!s_sleep.active || s_sleep.poweredOff)
    {
        return false;
    }

    s_sleep.totalTimeMs += deltaTimeMs;
    s_sleep.frameTimeMs += deltaTimeMs;

    if (s_sleep.frameTimeMs >= UI_SLEEP_FRAME_PERIOD_MS)
    {
        s_sleep.frameTimeMs -= UI_SLEEP_FRAME_PERIOD_MS;
        s_sleep.frame = (uint8_t)((s_sleep.frame + 1U) % 4U);
        changed = true;
    }

    if (s_sleep.totalTimeMs >= UI_SLEEP_POWER_OFF_MS)
    {
        u8g2_SetPowerSave(u8g2, 1U);
        s_sleep.poweredOff = true;
        changed = true;
    }

    return changed;
}

void UI_Sleep_Draw(u8g2_t *u8g2){
    if ((u8g2 == NULL) || s_sleep.poweredOff)
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1U);

    /*
     * 先画白色圆，再用黑色圆覆盖一部分，
     * 得到月牙效果。
     */
    u8g2_DrawDisc(u8g2, 42, 31, 16U, U8G2_DRAW_ALL);

    u8g2_SetDrawColor(u8g2, 0U);
    u8g2_DrawDisc(u8g2, 50, 24, 15U, U8G2_DRAW_ALL);

    u8g2_SetDrawColor(u8g2, 1U);
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tf);

    u8g2_DrawStr(u8g2, 74, 26, "Z");

    if (s_sleep.frame >= 1U)
    {
        u8g2_DrawStr(u8g2, 88, 18, "z");
    }

    if (s_sleep.frame >= 2U)
    {
        u8g2_DrawStr(u8g2, 101, 11, ".");
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tf);
    u8g2_DrawStr(u8g2, 75, 57, "B4 WAKE");
}

void UI_Sleep_Wake(u8g2_t *u8g2){
    if (u8g2 == NULL)
    {
        return;
    }

    u8g2_SetPowerSave(u8g2, 0U);

    s_sleep.active = false;
    s_sleep.poweredOff = false;
    s_sleep.frame = 0U;
}

bool UI_Sleep_IsPoweredOff(void){
    return s_sleep.poweredOff;
}
