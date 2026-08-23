#include "app/app_ui_task.h"

#include "app/menu.h"
#include "app/ui_display_port.h"

#include "bsp/bsp_button.h"
#include "bsp/bsp_buzzer.h"
#include "bsp/bsp_spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_UI_TASK_PERIOD_MS 20U   //UI动画和按键扫描周期
//UI中使用了浮点动画计算，栈空间不能设置得过小
//xTaskCreate()中的栈深度单位是StackType_t，不一定是字节。
#define APP_UI_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE + 512U)
#define APP_UI_TASK_PRIORITY (tskIDLE_PRIORITY + 2U)  //

static void app_ui_task(void *parameter);

static bool app_ui_handle_button_events(uint32_t events){
    //如果同义词扫描检测到多个按键，只处理其中一个，放置一次扫描同时执行多个菜单动作
    if ((events & BSP_BUTTON_1_EVENT) != 0U){
        return Menu_HandleKey(MENU_KEY_UP);
    }

    if ((events & BSP_BUTTON_2_EVENT) != 0U){
        return Menu_HandleKey(MENU_KEY_DOWN);
    }

    if ((events & BSP_BUTTON_3_EVENT) != 0U){
        return Menu_HandleKey(MENU_KEY_OK);
    }

    if ((events & BSP_BUTTON_4_EVENT) != 0U){
        return Menu_HandleKey(MENU_KEY_BACK);
    }

    return false;    
}

bool APP_UI_CreateTask(void){
    BaseType_t result;
    result = xTaskCreate(app_ui_task, "ui", APP_UI_TASK_STACK_DEPTH, NULL, APP_UI_TASK_PRIORITY, NULL);
    return (result == pdPASS);    
}

static void app_ui_task(void *parameter){
    TickType_t lastWakeTime;
    TickType_t previousUpdateTick;

    (void)parameter;

//SPI必须在OLED初始化之前完成初始化。
    BSP_SPI_Init();
    UI_Display_Init();

    BSP_Button_Init();
    BSP_Buzzer_Init();

    Menu_Init();

//先立即绘制第一帧，避免启动后等待一个任务周期。
    Menu_Render();
    UI_Display_Present();

    lastWakeTime = xTaskGetTickCount();
    previousUpdateTick = lastWakeTime;

    for (;;){
        uint32_t buttonEvents;
        TickType_t currentTick;
        TickType_t elapsedTicks;
        uint32_t deltaTimeMs;
        bool needRedraw;

        needRedraw = false;

        /*
         * 每20ms调用一次。
         * 当前按键驱动连续3次状态一致才确认，
         * 因此实际消抖时间约为60ms。
         */
        buttonEvents = BSP_Button_Scan();

        if (app_ui_handle_button_events(buttonEvents)){
            needRedraw = true;
        }

        /*
         * 在处理完按键后重新读取时间。
         * 即使BSP_Buzzer_PlayTone()阻塞了50ms，
         * 动画也能获得接近真实的时间间隔。
         */
        currentTick = xTaskGetTickCount();
        elapsedTicks = currentTick - previousUpdateTick;
        previousUpdateTick = currentTick;

        deltaTimeMs = (uint32_t)(((uint64_t)elapsedTicks * 1000ULL) / (uint64_t)configTICK_RATE_HZ);

        if (Menu_Update(deltaTimeMs)){
            needRedraw = true;
        }

        if (needRedraw){
            Menu_Render();
            UI_Display_Present();
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_UI_TASK_PERIOD_MS));
    }    
}