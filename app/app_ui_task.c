#include "app/app_ui_task.h"
#include "app/app_temp_task.h"
#include "app/app_buzzer_task.h"
#include "app/app_motor_task.h"
#include "app/app_encoder_task.h"

#include "UI/menu.h"
#include "UI/ui_display_port.h"
#include "UI/ui_waveform.h"
#include "UI/ui_encoder_waveform.h"

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

#define APP_UI_KEY_TONE_HZ 2000U
#define APP_UI_KEY_TONE_MS 50U

static void app_ui_task(void *parameter);

/*
将 UI 选择转换为电机模块的方向指令,普通刷新时仅在选择改变后下发
*/
// static void app_ui_sync_motor_direction(bool force){
//     static menu_foc_direction_t lastDirection = MENU_FOC_DIRECTION_POSITIVE;
//     menu_foc_direction_t direction;

//     direction = Menu_GetFocDirection();
//     if ((!force) && (direction == lastDirection)){
//         return;
//     }
//     APP_Motor_SetDirection((direction == MENU_FOC_DIRECTION_NEGATIVE) ? DIRECTION_NEGATIVE : DIRECTION_POSITIVE);

//     lastDirection = direction;
// }
static void app_ui_sync_motor(void){
    menu_foc_mode_t uiMode = Menu_GetFocMode();
    app_motor_mode_t mode = APP_MOTOR_OFF;

    int8_t direction =
        (Menu_GetFocDirection() == MENU_FOC_DIRECTION_NEGATIVE) ?
        DIRECTION_NEGATIVE :
        DIRECTION_POSITIVE;

    switch (uiMode){
        case MENU_FOC_OPENLOOP:
            mode = APP_MOTOR_OPENLOOP;
            break;

        case MENU_FOC_POSITION:
            mode = APP_MOTOR_POSITION;
            break;

        case MENU_FOC_OFF:
        default:
            mode = APP_MOTOR_OFF;
            break;
    }

    APP_Motor_SetControl(mode, direction, Menu_GetFocRpm());
}
static bool app_ui_handle_button_events(uint32_t events){
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

    app_ui_sync_motor();
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
    bool pair23Ready;
    const uint8_t *pendingPairFrame = NULL;

    (void)parameter;

    UI_Display_Init();
    //共享RESET已完成，BSP_2OLEDs_Init只初始化2，3号控制器，不再复位
    pair23Ready = BSP_2OLEDS_Init();
    BSP_Button_Init();
    //BSP_Buzzer_Init();
    Menu_Init();
    UI_EncoderWave_SetFilterTimeMs(APP_ENCODER_TF_MS);
    app_ui_sync_motor();

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
        //独立消费温度和编码器队列，采集频率互不影响。
        //没有其他任务修改波形，因此ui_waveform内部不需要互斥锁
        for (;;){
            app_temp_sample_t temperatureSample;

            if (!APP_Temp_TryReceive(&temperatureSample)){
                break;
            }

            if (UI_Waveform_AddTemperatureSample(temperatureSample.temperatureQ4, temperatureSample.valid)){
                needRedraw = true;
            }
        }
        for (;;){
            app_encoder_sample_t sample;

            if (!APP_Encoder_TryReceive(&sample)){
                break;
            }

            UI_EncoderWave_AddSample(sample.rawRpm, sample.filteredRpm, sample.sequence, sample.valid);
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
            if (pair23Ready && Menu_PreparePairFrame(&pairFrame)){
                pendingPairFrame = pairFrame;
            }
        }
        //失败的波形帧或清屏帧下一轮重试；新页面的帧优先替换旧帧。
        if (pair23Ready && pendingPairFrame != NULL){
            if (BSP_2OLEDS_Present(pendingPairFrame)){
                pendingPairFrame = NULL;
            }
        }
        if (needRedraw){
            //1号屏最后刷新。
            UI_Display_Present();
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_UI_TASK_PERIOD_MS));
    }    
}
