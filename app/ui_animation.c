#include "app/ui_animation.h"

#include <math.h>
#include <stddef.h>

//UI动画的结束阈值，如果当前值和目标值之差小于该阈值，认为动画已经接近目标值，可以直接设置为目标值。
#define UI_ANIMATION_END_THRESHOLD 0.10f 

/* 动画帧函数
 * 传入current指针，在函数中修改current原变量的值 
 * 让current逐渐接近target。
 * deltaTimeMs：距离上一帧经过的时间。
 * responseTimeMs：动画响应时间，越小移动越快。
 *
 * 返回true表示当前值发生了变化。
 */
bool UI_Animation_Approach(float *current, float target, uint32_t deltaTimeMs, uint32_t responseTimeMs){
    float difference;
    float factor;

    if (current == NULL){
        return false;
    }
    difference = target - *current; //当前值距离目标值的差异
    if (fabsf(difference) <= UI_ANIMATION_END_THRESHOLD){//如果差异小于阈值，直接设置目标值
        if(*current != target){
            *current = target;
            return true;
        }
        return false;
    }
    if (responseTimeMs == 0U){ // 设置为0，没有过渡
        *current = target;
        return true;
    }

    factor = (float)deltaTimeMs / (float)responseTimeMs; //计算当前帧的移动因子
    if(factor > 1.0f){
        factor = 1.0f; //限制因子最大为1，避免过度移动
    }
    *current += difference * factor; //根据差异和因子计算新的当前值
    return true;
}

