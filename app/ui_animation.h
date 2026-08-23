#ifndef UI_ANIMATION_H
#define UI_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 让current逐渐接近target。
 * deltaTimeMs：距离上一帧经过的时间。
 * responseTimeMs：动画响应时间，越小移动越快。
 *
 * 返回true表示当前值发生了变化。
 */
bool UI_Animation_Approach(float *current, float target, uint32_t deltaTimeMs, uint32_t responseTimeMs);


#ifdef __cplusplus
}
#endif


#endif