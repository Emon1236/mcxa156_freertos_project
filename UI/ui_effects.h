#ifndef UI_EFFECTS_H
#define UI_EFFECTS_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_Effects_FadeBegin(uint32_t stepMs, bool toWhite);   //启动淡出动画
bool UI_Effects_FadeUpdate(uint32_t deltaTimeMs);   //更新淡出动画状态
bool UI_Effects_FadeIsFinished(void);   //判断淡出动画是否完成
bool UI_Effects_FadeRunning(void);   //判断淡出动画是否运行
void UI_Effects_ApplyFade(u8g2_t *u8g2);    //应用淡出动画等级到u8g2帧缓冲
void UI_Effects_FadeReset(void);   //重置淡出动画状态
void UI_Effects_ApplyBokeh(u8g2_t *u8g2);   //对帧缓冲应用棋盘式稀疏处理


#ifdef __cplusplus
}
#endif

#endif
