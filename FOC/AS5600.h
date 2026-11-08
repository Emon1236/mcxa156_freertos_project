#ifndef AS5600_H
#define AS5600_H

#include <stdint.h>
#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AS5600_COUNTS_PER_REVOLUTION 4096U

//模块初始化
status_t AS5600_Init(void);
//读取12位原始机械角度，范围0~4095
status_t AS5600_ReadRawAngle(uint16_t *rawAngle);
//读取角度并转换为角度制，范围0.0f~360.0f
status_t AS5600_ReadAngleDegrees(float *angleDegrees);
//读取角度并转换为毫度，范围0~360000，不适用浮点运算，适合串口测试或后续定点算法
status_t AS5600_ReadAngleMilliDegrees(uint32_t *angleMilliDegrees);

status_t AS5600_CheckMagnet(void);

#ifdef __cplusplus
}
#endif

#endif // AS5600_H