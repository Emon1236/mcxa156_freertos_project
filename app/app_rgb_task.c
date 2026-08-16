#include "app/app_rgb_task.h"
#include "bsp/bsp_rgb_led.h"
#include "FreeRTOS.h"
#include "task.h"

#define RGB_TASK_STACK_PERIOD_MS 1000U //这个U是C语言中的usigned int类型后缀，UL则表示usinged long类型。
//uint32_t period_ms = 1000U; //U和uint32_t不是等价关系，而是配合关系，当然U也可以不加

void APP_RGB_Task(void *pvParameters){
    TickType_t xLastWakeTime;

    (void) pvParameters;

    BSP_RGB_LED_Init();
    
    xLastWakeTime = xTaskGetTickCount();

    for(;;){
        BSP_RGB_SetColor(BSP_RGB_COLOR_RED);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(RGB_TASK_STACK_PERIOD_MS));
        BSP_RGB_SetColor(BSP_RGB_COLOR_GREEN);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(RGB_TASK_STACK_PERIOD_MS));
        BSP_RGB_SetColor(BSP_RGB_COLOR_BLUE);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(RGB_TASK_STACK_PERIOD_MS));
    }
    //使用vTaskDelayUntil()使用固定的时间基准，确保每次任务执行的时间间隔为RGB_TASK_STACK_PERIOD_MS毫秒。
}