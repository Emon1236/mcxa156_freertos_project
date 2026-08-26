#ifndef UI_DISPLAY_PORT_H
#define UI_DISPLAY_PORT_H

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_DISPLAY_WIDTH  128U
#define UI_DISPLAY_HEIGHT 64U

void UI_Display_Init(void);
void UI_Display_Clear(void);
void UI_Display_Present(void);

u8g2_t *UI_Display_GetContext(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_DISPLAY_PORT_H */