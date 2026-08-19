#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_BUZZER_RESONANT_FREQUENCY_HZ 4000U //4kHz

void BSP_Buzzer_Init(void);
void BSP_Buzzer_Stop(void);

void BSP_Buzzer_PlayTone(uint32_t frequencyHz, uint32_t durationMs);

void BSP_Buzzer_PlaySweep(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUZZER_H */
