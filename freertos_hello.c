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

#include "app/app_ui_task.h"
#include "bsp/bsp_2oleds.h"
#include "bsp/bsp_spi.h"

static uint8_t s_oled23TestFrame[BSP_2OLEDS_FRAME_BYTES];

static void oled23TestFrame(void){
    uint16_t page;
    uint16_t x;
    for (page = 0U; page < BSP_2OLEDS_PAGE_COUNT; page++){
        for (x =0U; x < BSP_2OLEDS_WIDTH; x++){
            uint8_t *pixel = &s_oled23TestFrame[page * BSP_2OLEDS_WIDTH + x];/* 设置像素值 */
            if (x < 128){
                //左屏幕横向条纹
                *pixel = (page & 1U) != 0U ? 0xFFU : 0x00U;
            }
            else{
                //右屏幕竖向条纹
                *pixel = (x & 3U) != 0U ? 0xFFU : 0x00U;
            }
        }
    }
}
static void oled23_test_task(void *parameter)
{
    bool presentSucceeded;

    (void)parameter;

    oled23TestFrame();

    for (;;)
    {
        presentSucceeded = BSP_2OLEDS_Present(s_oled23TestFrame);

        /*
         * 即使一次发送失败，也周期性重试。
         * 调试时可在此处给 presentSucceeded 设置断点观察。
         */
        (void)presentSucceeded;

        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

int main(void)
{

    BOARD_InitHardware();
    BSP_SPI_Init();
    if (!BSP_2OLEDS_Init())
    {
        PRINTF("Failed to initialize 2OLEDs.\r\n");
        for (;;)
        {
        }
    }
    // if (!APP_UI_CreateTask())
    // {
    //     /*
    //      * 任务创建失败一般表示FreeRTOS堆空间不足。
    //      * 此处停机，方便调试器定位。
    //      */
    //     for (;;)
    //     {
    //     }
    // }
    if (xTaskCreate(oled23_test_task,
                    "oled23_test",
                    configMINIMAL_STACK_SIZE + 128U,
                    NULL,
                    tskIDLE_PRIORITY + 1U,
                    NULL) != pdPASS)
    {
        PRINTF("Failed to create oled23_test_task.\r\n");
        for (;;)
        {
        }
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}
