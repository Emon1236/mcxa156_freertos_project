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
#include <stdint.h>

#define OLED_TEST_PANEL_INDEX  3U
#define OLED_TEST_DELAY_MS     1500U

static void OLED_TestRefresh(const char *testName)
{
    BSP_OLED_SelectPanel(OLED_TEST_PANEL_INDEX);
    BSP_OLED_Refresh();

    PRINTF("OLED test: %s\r\n", testName);
    vTaskDelay(pdMS_TO_TICKS(OLED_TEST_DELAY_MS));
}

static void OLED_TestTask(void *param)
{
    (void)param;

    PRINTF("OLED panel %u test start.\r\n",
           (unsigned int)OLED_TEST_PANEL_INDEX);

    BSP_OLED_Init(OLED_TEST_PANEL_INDEX);

    while (1)
    {
        /*
         * 测试1：ShowChar正常使用
         *
         * 预期：
         * 第一行显示6x8字符：A 0 z ?
         * 第二、三行显示8x16字符：B 8 @
         * \x01不在字库范围内，应显示为问号
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowChar(0U,  0U, 'A', OLED_FONT_6X8);
        BSP_OLED_ShowChar(12U, 0U, '0', OLED_FONT_6X8);
        BSP_OLED_ShowChar(24U, 0U, 'z', OLED_FONT_6X8);
        BSP_OLED_ShowChar(36U, 0U, '\x01', OLED_FONT_6X8);

        BSP_OLED_ShowChar(0U, 16U, 'B', OLED_FONT_8X16);
        BSP_OLED_ShowChar(16U, 16U, '8', OLED_FONT_8X16);
        BSP_OLED_ShowChar(32U, 16U, '@', OLED_FONT_8X16);

        OLED_TestRefresh("1 ShowChar normal");

        /*
         * 测试2：ShowChar合法边界
         * 6x8右下角最后一个完整合法坐标：
         * x = 128 - 6 = 122
         * y = 64 - 8  = 56
         * 8x16右下角最后一个完整合法坐标：
         * x = 128 - 8  = 120
         * y = 64 - 16  = 48
         * 预期：四角字符完整显示，没有越界、错位。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowChar(0U,   0U,  'A', OLED_FONT_6X8);
        BSP_OLED_ShowChar(122U, 0U,  'B', OLED_FONT_6X8);
        BSP_OLED_ShowChar(0U,   56U, 'C', OLED_FONT_6X8);
        BSP_OLED_ShowChar(122U, 56U, 'D', OLED_FONT_6X8);

        BSP_OLED_ShowChar(56U,  16U, 'M', OLED_FONT_8X16);
        BSP_OLED_ShowChar(120U, 48U, 'Z', OLED_FONT_8X16);

        OLED_TestRefresh("2 ShowChar boundary");

        /*
         * 测试3：ShowChar裁剪
         * 预期：
         * x=125时，6x8字符只显示屏幕内剩余3列。
         * y=60时，6x8字符只显示屏幕内剩余4行。
         * 8x16字符同样只显示屏幕内部分。
         * 重点观察：
         * 不应出现数组越界、死机或屏幕其他区域被破坏。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowChar(125U, 0U,  'W', OLED_FONT_6X8);
        BSP_OLED_ShowChar(0U,   60U, 'H', OLED_FONT_6X8);
        BSP_OLED_ShowChar(124U, 52U, 'X', OLED_FONT_8X16);

        OLED_TestRefresh("3 ShowChar clipping");

        /*
         * 测试4：ShowString正常使用
         * 预期：
         * 正确显示大小写字母、数字、符号。
         * \n之后从传入的起始x坐标开始换行。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowString(0U,0U,"Hello OLED 123!",OLED_FONT_6X8);
        BSP_OLED_ShowString(0U,16U,"ABC123",OLED_FONT_8X16);
        BSP_OLED_ShowString(0U,40U,"Line1\nLine2",OLED_FONT_6X8);

        OLED_TestRefresh("4 ShowString normal");

        /*
         * 测试5：ShowString整行极限
         * 6x8字体：128 / 6 = 21个完整字符，使用宽度126像素。
         * 8x16字体：128 / 8 = 16个完整字符，刚好占满128像素。
         * 预期：
         * 第一行显示21个字符，右侧剩余2列。
         * 8x16字符串刚好贴住屏幕右边界。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowString(0U,0U,"123456789012345678901",OLED_FONT_6X8);
        BSP_OLED_ShowString(0U,16U,"ABCDEFGHIJKLMNOP",OLED_FONT_8X16);

        OLED_TestRefresh("5 ShowString exact width");

        /*
         * 测试6：ShowString自动换行
         * 第一串有22个字符：前21个字符位于第一行，第22个字符应自动换到第二行。
         * 第二串使用8x16字体：前16个字符占满一行，Q应换到下一行。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowString(0U,0U,"123456789012345678901X",OLED_FONT_6X8);
        BSP_OLED_ShowString(0U,24U,"ABCDEFGHIJKLMNOPQ",OLED_FONT_8X16);

        OLED_TestRefresh("6 ShowString wrapping");

        /*
         * 测试7：ShowString屏幕底部极限
         * 6x8字体最后一个完整起始y为56。
         * 8x16字体最后一个完整起始y为48。
         * 预期："Bottom"完整显示在最后8行。"EDGE"完整显示在最后16行。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowString(0U,56U,"Bottom",OLED_FONT_6X8);
        BSP_OLED_ShowString(80U,48U,"EDGE",OLED_FONT_8X16);

        OLED_TestRefresh("7 ShowString bottom edge");

        /*
         * 测试8：ShowNumber正常使用
         * 预期：0必须正确显示。
         * 普通数字顺序不能颠倒。
         * 两种字体显示内容一致。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowNumber(0U,0U,0U,OLED_FONT_6X8);
        BSP_OLED_ShowNumber(18U,0U,12345U,OLED_FONT_6X8);
        BSP_OLED_ShowNumber(0U,16U,67890U,OLED_FONT_8X16);

        OLED_TestRefresh("8 ShowNumber normal");

        /*
         * 测试9：ShowNumber最大值
         * uint32_t最大值为4294967295，共10位。
         * 6x8占60像素。从x=68开始，右边界刚好为128。
         * 8x16占80像素。从x=48开始，右边界刚好为128。
         * 预期：两行都完整显示4294967295，且刚好贴住右边界。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowNumber(68U,0U,UINT32_MAX,OLED_FONT_6X8);
        BSP_OLED_ShowNumber(48U,16U,UINT32_MAX,OLED_FONT_8X16);

        OLED_TestRefresh("9 ShowNumber UINT32_MAX");

        /*
         * 测试10：ShowNumber横向截断
         * 6x8从x=110开始，只能完整显示3个字符。
         * 8x16从x=104开始，只能完整显示3个字符。
         * 按当前实现，预期两处都只显示最前面的"123"。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowNumber(110U,0U,1234567890U,OLED_FONT_6X8);
        BSP_OLED_ShowNumber(104U,16U,1234567890U,OLED_FONT_8X16);

        OLED_TestRefresh("10 ShowNumber clipping");

        /*
         * 测试11：组合使用
         * 用于观察多次调用是否互相覆盖、错位。
         */
        BSP_OLED_Clear();

        BSP_OLED_ShowString(0U,0U,"TEMP:",OLED_FONT_6X8);
        BSP_OLED_ShowNumber(30U,0U,25U,OLED_FONT_6X8);
        BSP_OLED_ShowChar(42U,0U,'C',OLED_FONT_6X8);
        BSP_OLED_ShowString(0U,16U,"COUNT",OLED_FONT_8X16);
        BSP_OLED_ShowNumber(48U,16U,987654U,OLED_FONT_8X16);

        OLED_TestRefresh("11 combined");
        /*
         * 测试12：清屏确认
         * 预期：屏幕完全熄灭。
         */
        BSP_OLED_Clear();
        OLED_TestRefresh("12 clear");
    }
}
int main(void)
{
    /* 硬件初始化. */
    BOARD_InitHardware();
    /* RGB初始化. */
    BSP_RGB_LED_Init();

    BSP_SPI_Init();
    //BSP_OLED_Init(0U);      /* 选中1号屏并初始化 */

    if (xTaskCreate(OLED_TestTask,
                    "OLED_Test",
                    configMINIMAL_STACK_SIZE + 256U,
                    NULL,
                    tskIDLE_PRIORITY + 2U,
                    NULL) != pdPASS)
    {
        PRINTF("OLED test task create failed!\r\n");
        for (;;)
        {
        }
    }
      

    if(xTaskCreate(APP_R_500ms_Task,"R_500ms_Task",configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1U,NULL) != pdPASS){
        PRINTF("R_500ms_Task create failed!\r\n");
        for(;;);
    }
    if(xTaskCreate(APP_G_2S_Task,"G_2S_Task",configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1U,NULL) != pdPASS){
        PRINTF("G_2S_Task create failed!\r\n");
        for(;;);
    }

    vTaskStartScheduler();
    for(;;);
}

