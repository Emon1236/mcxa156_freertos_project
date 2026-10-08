#include "app/app_ui_task.h"
#include "app/app_buzzer_task.h"

#include "UI/menu.h"
#include "UI/ui_display_port.h"

#include "bsp/bsp_button.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_UI_TASK_PERIOD_MS 20U   //UI动画和按键扫描周期
//UI中使用了浮点动画计算，栈空间不能设置得过小
//xTaskCreate()中的栈深度单位是StackType_t，不一定是字节。
#define APP_UI_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE + 512U)
#define APP_UI_TASK_PRIORITY (tskIDLE_PRIORITY + 2U)  //

#define APP_UI_KEY_TONE_HZ 2000U
#define APP_UI_KEY_TONE_MS 50U

static void app_ui_task(void *parameter);

static bool app_ui_handle_button_events(uint32_t events)
{
    menu_key_t key;
    menu_key_result_t result;
    //一轮扫描只处理一个按键。优先顺序：1、2、3、4。
    if ((events & BSP_BUTTON_1_EVENT) != 0U){
        key = MENU_KEY_UP;
    }
    else if ((events & BSP_BUTTON_2_EVENT) != 0U){
        key = MENU_KEY_DOWN;
    }
    else if ((events & BSP_BUTTON_3_EVENT) != 0U){
        key = MENU_KEY_OK;
    }
    else if ((events & BSP_BUTTON_4_EVENT) != 0U){
        key = MENU_KEY_BACK;
    }
    else{
        return false;
    }
    /* UI 负责菜单状态和交互结果。 */
    result = Menu_HandleKey(key);
    /* App 将反馈请求转换为实际声音请求。 */
    if (result.requestKeyFeedback){
        //非阻塞提交,提示音提交失败不影响菜单操作和重绘。
        (void)APP_Buzzer_PlayTone(APP_UI_KEY_TONE_HZ, APP_UI_KEY_TONE_MS);
    }

    return result.needRedraw;
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

    UI_Display_Init();
    BSP_Button_Init();
    //BSP_Buzzer_Init();
    Menu_Init();

    //先立即绘制第一帧，避免启动后等待一个任务周期。
    Menu_Render();
    UI_Display_Present();

    lastWakeTime = xTaskGetTickCount();
    previousUpdateTick = lastWakeTime;
    //UI主循环代码
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