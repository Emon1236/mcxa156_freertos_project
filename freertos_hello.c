/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"

#include "bsp/bsp_spi.h"
#include "bsp/bsp_4oleds.h"


#include "bsp/bsp_button.h"
#include "bsp/bsp_buzzer.h"

#include "app/menu.h"

#define BUTTON_TASK_PERIOD_MS 10U
#define MENU_QUEUE_LENGTH     8U

#define BUTTON_TASK_PRIORITY  (tskIDLE_PRIORITY + 2U)
#define MENU_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)

#define BUTTON_TASK_STACK_SIZE \
    (configMINIMAL_STACK_SIZE + 64U)

#define MENU_TASK_STACK_SIZE \
    (configMINIMAL_STACK_SIZE + 256U)

static QueueHandle_t s_menuKeyQueue;

/* 向菜单队列发送一个按键事件。 */
static void Menu_SendKey(menu_key_t key)
{
    /*
     * 等待时间为0，队列满时直接放弃。
     * 正常人工按键速度下，长度为8的队列已经足够。
     */
    (void)xQueueSend(s_menuKeyQueue, &key, 0U);
}

static void ButtonTask(void *pvParameters)
{
    TickType_t lastWakeTime;

    (void)pvParameters;

    lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        uint32_t events = BSP_Button_Scan();

        if ((events & BSP_BUTTON_1_EVENT) != 0U)
        {
            Menu_SendKey(MENU_KEY_UP);
        }

        if ((events & BSP_BUTTON_2_EVENT) != 0U)
        {
            Menu_SendKey(MENU_KEY_DOWN);
        }

        if ((events & BSP_BUTTON_3_EVENT) != 0U)
        {
            Menu_SendKey(MENU_KEY_OK);
        }

        if ((events & BSP_BUTTON_4_EVENT) != 0U)
        {
            Menu_SendKey(MENU_KEY_BACK);
        }

        /*
         * BSP_Button_Scan要求每10ms扫描一次。
         * 连续3次检测到相同的新状态后确认按键，
         * 因此消抖时间约为30ms。
         */
        vTaskDelayUntil(&lastWakeTime,
                        pdMS_TO_TICKS(BUTTON_TASK_PERIOD_MS));
    }
}

static void MenuTask(void *pvParameters)
{
    menu_key_t key;

    (void)pvParameters;

    Menu_Init();
    Menu_Render();

    for (;;)
    {
        /*
         * 没有按键时一直阻塞在队列上，不消耗CPU。
         */
        if (xQueueReceive(s_menuKeyQueue,
                          &key,
                          portMAX_DELAY) == pdPASS)
        {
            if (Menu_HandleKey(key))
            {
                Menu_Render();
            }
        }
    }
}

int main(void)
{
    BOARD_InitHardware();

    BSP_SPI_Init();
    BSP_4OLEDs_Init();
    BSP_Button_Init();
    BSP_Buzzer_Init();

    s_menuKeyQueue =
        xQueueCreate(MENU_QUEUE_LENGTH,
                     sizeof(menu_key_t));

    configASSERT(s_menuKeyQueue != NULL);

    configASSERT(
        xTaskCreate(ButtonTask,
                    "button",
                    BUTTON_TASK_STACK_SIZE,
                    NULL,
                    BUTTON_TASK_PRIORITY,
                    NULL) == pdPASS);

    configASSERT(
        xTaskCreate(MenuTask,
                    "menu",
                    MENU_TASK_STACK_SIZE,
                    NULL,
                    MENU_TASK_PRIORITY,
                    NULL) == pdPASS);

    vTaskStartScheduler();

    for (;;)
    {
    }
}
