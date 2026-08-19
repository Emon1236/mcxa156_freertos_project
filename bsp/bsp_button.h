#ifndef BSP_BUTTON_H_
#define BSP_BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define BSP_BUTTON_COUNT 4U
/* BSP_Button_Scan()建议每10ms调用一次，连续3次检测到新状态后确认，等价消抖30ms */
#define BSP_BUTTON_DEBOUNCE_COUNT 3U

typedef enum{
    BSP_BUTTON_1 = 0U,  //P2_1
    BSP_BUTTON_2,       //P2_2
    BSP_BUTTON_3,       //P2_4
    BSP_BUTTON_4        //P2_5
} bsp_button_t;

//按键事件位
#define BSP_BUTTON_1_EVENT (1UL << BSP_BUTTON_1)
#define BSP_BUTTON_2_EVENT (1UL << BSP_BUTTON_2)
#define BSP_BUTTON_3_EVENT (1UL << BSP_BUTTON_3)
#define BSP_BUTTON_4_EVENT (1UL << BSP_BUTTON_4)

void BSP_Button_Init(void);
bool BSP_Button_Ispressed(bsp_button_t button);
uint32_t BSP_Button_Scan(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUTTON_H_ */
