#ifndef BSP_P3T1755_H
#define BSP_P3T1755_H

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

status_t BSP_P3T1755_Init(void);
status_t BSP_P3T1755_ReadTemperatureC(float *temperatureC);


#ifdef __cplusplus
}
#endif

#endif