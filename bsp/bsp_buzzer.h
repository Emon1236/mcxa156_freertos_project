#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdint.h>
#include "fsl_common.h" 

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_BUZZER_MIN_FREQUENCY_HZ       50U
#define BSP_BUZZER_MAX_FREQUENCY_HZ      10000U
#define BSP_BUZZER_RESONANT_FREQUENCY_HZ 4000U //4kHz

status_t BSP_Buzzer_Init(void);

/* 持续输出指定频率的 PWM，直到调用 Stop。 */
status_t BSP_Buzzer_Start(uint32_t frequencyHz);

/* 关闭输出，并保证引脚为低电平。 */
void BSP_Buzzer_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUZZER_H */
