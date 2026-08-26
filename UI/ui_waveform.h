#ifndef UI_WAVEFORM_H
#define UI_WAVEFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

typedef bool (*ui_waveform_sample_function_t)(uint16_t *millivolts);

void UI_Waveform_Init(ui_waveform_sample_function_t sampleFunction);
void UI_Waveform_Reset(void);
bool UI_Waveform_Update(uint32_t deltaTimeMs);
void UI_Waveform_Draw(u8g2_t *u8g2);
bool UI_Waveform_HasSignalSource(void);

#endif


