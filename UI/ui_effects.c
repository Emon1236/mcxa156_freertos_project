/*
淡出动画效果功能维护
*/
#include "ui_effects.h"

//淡出动画状态结构体
typedef struct {
    bool running;
    bool finished;
    bool toWhite;       //是否淡出到白色

    uint8_t step;       //当前淡出等级
    uint32_t elapsedMs; //当前淡出等级已持续时间
    uint32_t stepTimeMs;//每个淡出等级持续时间
} ui_fade_state_t;  

static ui_fade_state_t s_fade;

static uint16_t ui_effects_get_buffer_size(const u8g2_t *u8g2){
    return (uint16_t)(8U * u8g2_GetBufferTileWidth(u8g2) * u8g2_GetBufferTileHeight(u8g2));
}
/**
 * @brief 开始淡出动画
 * 
 * @param stepMs 每个淡出等级持续时间，单位毫秒
 * @param toWhite 是否淡出到白色：false表示淡出到黑色，true表示淡出到白色
 */
void UI_Effects_FadeBegin(uint32_t stepMs, bool toWhite){
    if (stepMs == 0U){
        stepMs = 1U;
    }
    s_fade.running = true;
    s_fade.finished = false;
    s_fade.toWhite = toWhite;
    s_fade.step = 0U;
    s_fade.elapsedMs = 0U;
    s_fade.stepTimeMs = stepMs;
}
/**
 * @brief 更新淡出动画状态
 * 
 * @param deltaTimeMs 自上次更新以来的时间增量，单位毫秒
 * @return true 表示淡出等级发生变化
 * @return false 淡出等级未变化
 */
bool UI_Effects_FadeUpdate(uint32_t deltaTimeMs){
    bool changed =false;
    if(!s_fade.running || s_fade.finished){
        return false;
    }
    s_fade.elapsedMs += deltaTimeMs;

    while((s_fade.elapsedMs >= s_fade.stepTimeMs) && (s_fade.step < 4U)){
        s_fade.elapsedMs -= s_fade.stepTimeMs;
        s_fade.step++;
        changed = true;
    }
    if (s_fade.step >= 4U){
        s_fade.finished = true;
    }
    return changed;
}
bool UI_Effects_FadeIsFinished(void){
    return s_fade.finished;
}

bool UI_Effects_FadeIsRunning(void){
    //只有两个状态就没必要与finished比较了
    return s_fade.running;
}

 void UI_Effects_FadeReset(void){
    s_fade.running = false;
    s_fade.finished = false;
    s_fade.toWhite = false;
    s_fade.step = 0U;
    s_fade.elapsedMs = 0U;
    s_fade.stepTimeMs = 0U;
 }

 void UI_Effects_ApplyFade(u8g2_t *u8g2){
    uint8_t *buffer;
    uint16_t bufferSize;
    uint16_t i;
    if (u8g2 == NULL || !s_fade.running || s_fade.step == 0U){
        return;
    }
    buffer = u8g2_GetBufferPtr(u8g2);   //获取显示缓冲区指针
    bufferSize = ui_effects_get_buffer_size(u8g2);  //获取显示缓冲区大小

    if (!s_fade.toWhite){
        for(i=0U; i< bufferSize; i++){
            if(s_fade.step >= 4U){
                buffer[i] = 0x00;  //淡出到黑色
            }
            else if ((i&1U) != 0U){
                buffer[i] = (s_fade.step >= 2U)?0x00U:(uint8_t)(buffer[i] & 0xAAU);
            }
            else if (s_fade.step >= 3U){
                buffer[i] &= 0x55U;
            }
        }
    }
    else{
        for (i=0U; i < bufferSize; i++){
            if(s_fade.step >= 4U){
                buffer[i] = 0xFFU;  //淡出到白色
            }
            else if ((i & 1U) != 0U){
                buffer[i] = (s_fade.step >= 2U)?0xFFU:(uint8_t)(buffer[i] | 0xAAU);
            }
            else if (s_fade.step >= 3U){
                buffer[i] |= 0x55U;
            }
        }
    }
}
/**
 * @brief 应用散景效果到显示缓冲区
 * 
 * @param u8g2 显示缓冲区对象
 */
void UI_Effects_ApplyBokeh(u8g2_t *u8g2){
    uint8_t *buffer;
    uint16_t bufferSize;
    uint16_t i;

    if(u8g2 == NULL){
        return;
    }
    buffer = u8g2_GetBufferPtr(u8g2);
    bufferSize = ui_effects_get_buffer_size(u8g2);
    for (i = 0U; i < bufferSize; i++){
        buffer[i] &= ((i & 1U) != 0U) ? 0x55U:0xAAU;
    }
}

