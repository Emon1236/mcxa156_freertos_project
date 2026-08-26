#ifndef UI_SLEEP_H
#define UI_SLEEP_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_Sleep_Enter(u8g2_t *u8g2);
bool UI_Sleep_Update(u8g2_t *u8g2, uint32_t deltaTimeMs);
void UI_Sleep_Draw(u8g2_t *u8g2);
void UI_Sleep_Wake(u8g2_t *u8g2);
bool UI_Sleep_IsPoweredOff(void);

#ifdef __cplusplus
}
#endif

#endif