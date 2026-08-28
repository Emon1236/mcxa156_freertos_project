#ifndef UI_WAVEFORM_H
#define UI_WAVEFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#define UI_WAVEFORM_SAMPLE_CAPACITY 256U    // 波形样本容量
#define UI_WAVEFORM_FRAME_WIDTH     256U   // 波形显示帧宽度
#define UI_WAVEFORM_FRAME_HEIGHT    64U    // 波形显示帧高度
#define UI_WAVEFORM_FRAME_BYTES     UI_WAVEFORM_FRAME_WIDTH * UI_WAVEFORM_FRAME_HEIGHT / 8U   // 波形显示帧字节数

// UI波形显示配置
typedef enum{
    UI_WAVEFORM_SCALE_AUTO = 0U, // 自动缩放
    UI_WAVEFORM_SCALE_HOLD  // 手动缩放
} ui_waveform_scale_mode_t;

void UI_Waveform_Init(void);    //初始化波形模型，不删除后台采集的历史数据
bool UI_Waveform_AddTemperatureSample(int16_t temperatureQ4, bool valid);   //接收app_ui_task转交的温度数据
void UI_Waveform_Enter(void);
void UI_Waveform_Leave(void);
bool UI_Waveform_Update(uint32_t deltaTimeMs);
void UI_Waveform_DrawControl(u8g2_t *u8g2);
//手动缩小/扩大量程。调节后自动进入Hold模式
void UI_Waveform_ScaleIn(void);
void UI_Waveform_ScaleOut(void);

void UI_Waveform_ToggleAutoHold(void);   // 切换自动/手动缩放模式

bool UI_Waveform_PerpareFrame(const uint8_t **frameBuffer);   // 准备波形显示帧

bool UI_Waveform_IsActive(void);   // 判断波形显示是否处于活动状态

// typedef bool (*ui_waveform_sample_function_t)(uint16_t *millivolts);

// void UI_Waveform_Init(ui_waveform_sample_function_t sampleFunction);
// void UI_Waveform_Reset(void);
// bool UI_Waveform_Update(uint32_t deltaTimeMs);
// void UI_Waveform_Draw(u8g2_t *u8g2);
// bool UI_Waveform_HasSignalSource(void);

#endif


