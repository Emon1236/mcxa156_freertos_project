#ifndef UI_ENCODER_WAVEFORM_H
#define UI_ENCODER_WAVEFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#define UI_ENCODER_WAVE_WIDTH       256U
#define UI_ENCODER_WAVE_HEIGHT       64U
#define UI_ENCODER_WAVE_FRAME_BYTES 2048U

void UI_EncoderWave_Init(void);
void UI_EncoderWave_Enter(void);
void UI_EncoderWave_Leave(void);

/* 接收App传入的数据，不负责采集和滤波。 */
void UI_EncoderWave_AddSample(float rawRpm, float filteredRpm, uint32_t sequence, bool valid);

/* 仅更新界面上的参数说明。 */
void UI_EncoderWave_SetFilterTimeMs(uint32_t filterTimeMs);

bool UI_EncoderWave_Update(uint32_t deltaTimeMs);
void UI_EncoderWave_Scale(bool zoomIn);
void UI_EncoderWave_DrawControl(u8g2_t *u8g2);
bool UI_EncoderWave_PrepareFrame(const uint8_t **frameBuffer);

#endif
