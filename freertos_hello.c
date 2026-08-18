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

#include "bsp/bsp_rgb_led.h"
#include "app/app_rgb_task.h"

#include "bsp/bsp_spi.h"
#include "bsp/bsp_oled_12864.h"
#include "bsp/bsp_oled_data.h"
#include "bsp/bsp_button.h"
#include <stdint.h>

#define BUTTON_OLED_PANEL_INDEX 3U
#define BUTTON_SCAN_PERIOD_MS   10U

static void Button_OLED_DrawCounts(const uint8_t buttonCount[BSP_BUTTON_COUNT])
{
    static const char *const labels[BSP_BUTTON_COUNT] =
    {
        "b1:",
        "b2:",
        "b3:",
        "b4:"
    };

    BSP_OLED_Clear();

    for (uint8_t i = 0U; i < BSP_BUTTON_COUNT; i++)
    {
        uint8_t y = (uint8_t)(i * 16U);

        BSP_OLED_ShowString(0U, y, labels[i], OLED_FONT_8X16);
        BSP_OLED_ShowNumber(32U, y, buttonCount[i], OLED_FONT_8X16);
    }

    BSP_OLED_SelectPanel(BUTTON_OLED_PANEL_INDEX);
    BSP_OLED_Refresh();
}

static void Button_CountOLEDTask(void *param)
{
    uint8_t buttonCount[BSP_BUTTON_COUNT] = {0U, 0U, 0U, 0U};
    TickType_t lastWakeTime;

    (void)param;

    BSP_SPI_Init();
    BSP_Button_Init();
    BSP_OLED_Init(BUTTON_OLED_PANEL_INDEX);

    Button_OLED_DrawCounts(buttonCount);
    lastWakeTime = xTaskGetTickCount();

    while (1)
    {
        uint32_t pressedEvents = BSP_Button_Scan();

        if (pressedEvents != 0U)
        {
            for (uint8_t i = 0U; i < BSP_BUTTON_COUNT; i++)
            {
                if ((pressedEvents & (1UL << i)) != 0U)
                {
                    buttonCount[i]++;

                    PRINTF("Button %u count: %u\r\n",
                           (unsigned int)(i + 1U),
                           (unsigned int)buttonCount[i]);
                }
            }

            Button_OLED_DrawCounts(buttonCount);
        }

        vTaskDelayUntil(&lastWakeTime,
                        pdMS_TO_TICKS(BUTTON_SCAN_PERIOD_MS));
    }
}


int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();

    if (xTaskCreate(Button_CountOLEDTask,
                    "Button_OLED",
                    configMINIMAL_STACK_SIZE + 256U,
                    NULL,
                    tskIDLE_PRIORITY + 2U,
                    NULL) != pdPASS)
    {
        PRINTF("Button OLED task create failed!\r\n");

        for (;;)
        {
        }
    }

    vTaskStartScheduler();
    for(;;);
}

