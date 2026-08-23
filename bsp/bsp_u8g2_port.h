#ifndef BSP_U8G2_PORT_H
#define BSP_U8G2_PORT_H

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void BSP_U8G2_Init(void);

u8g2_t *BSP_U8G2_GetContext(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_U8G2_PORT_H */