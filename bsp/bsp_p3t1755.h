#ifndef BSP_P3T1755_H
#define BSP_P3T1755_H

#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

status_t BSP_P3T1755_Init(void);

//读取温度原始定点值，用于波形显示的精确计算
status_t BSP_P3T1755_ReadTemperatureQ4(int16_t *temperatureQ4);
//复用原始定点值接口，转换为摄氏温度
status_t BSP_P3T1755_ReadTemperatureC(float *temperatureC);


#ifdef __cplusplus
}
#endif

#endif