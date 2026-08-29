#include "app/app_ui_task.h"
#include "app/app_temp_task.h"
#include "UI/menu.h"
#include "UI/ui_display_port.h"
#include "UI/ui_waveform.h"

#include "bsp/bsp_button.h"
#include "bsp/bsp_buzzer.h"
#include "bsp/bsp_2oleds.h"

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
    bool pair23Ready;

    (void)parameter;

    UI_Display_Init();
    //共享RESET已完成，BSP_2OLEDs_Init只初始化2，3号控制器，不再复位
    pair23Ready = BSP_2OLEDS_Init();
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
        //将温度任务累积的消息全部取出
        //没有其他任务修改波形，因此ui_waveform内部不需要互斥锁
        for (;;){
            app_temp_sample_t temperatureSample;

            if (!APP_Temp_TryReceive(&temperatureSample)){
                break;
            }

            if (UI_Waveform_AddTemperatureSample(temperatureSample.temperatureQ4, temperatureSample.valid)){
                /*
                * 当前正显示温度控制页面时，
                * 温度变化需要刷新1号屏。
                */
                needRedraw = true;
            }
        }
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
            const uint8_t *pairFrame = NULL;
            // Menu_Render只在RAM中的U8G2帧缓冲里绘图，此时不占用SPI
            Menu_Render();
            //UI_Display_Present();
            //2，3屏幕刷新和1号相互独立
            if (pair23Ready && UI_Waveform_PerpareFrame(&pairFrame)){
                if (!BSP_2OLEDS_Present(pairFrame))
                {
                    /*
                    * 调试时在此处设置断点。
                    * 如果进入这里，需要继续检查SPI Busy或传输错误。
                    */
                }
            }
            //1号屏最后刷新。
        UI_Display_Present();
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_UI_TASK_PERIOD_MS));
    }    
}