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

#include "bsp/bsp_p3t1755.h"

static void APP_TemperatureSensorTest(void){
    status_t status;
    float temperatureC;

    int32_t temperatureMillic;
    uint32_t magnitude;

    status = BSP_P3T1755_Init();
    if (status != kStatus_Success){
        PRINTF("P3T1755 init failed, status=%d\r\n",
               (int)status);
        return;
    }
    status = BSP_P3T1755_ReadTemperatureC(
        &temperatureC);

    if (status != kStatus_Success)
    {
        PRINTF("P3T1755 read failed, status=%d\r\n",
               (int)status);
        return;
    }
    //将浮点温度转换成毫摄氏度
    temperatureMillic = (int32_t)(temperatureC * 1000.0f);
    if(temperatureMillic < 0){
        magnitude = (uint32_t)(-temperatureMillic);
        // float t = (float)magnitude / 1000.0f;
        //PRINTF("TemperatureF: %.3f Celsius\r\n", t);//CmakeLists.txt没有启用浮点支持
        PRINTF("Temperature: %u.%03u C\r\n",
               (unsigned int)(magnitude / 1000U),
               (unsigned int)(magnitude % 1000U));
    }
    else{
        magnitude = (uint32_t)temperatureMillic;
        // float t = (float)magnitude / 1000.0f;
        //PRINTF("TemperatureF: %.3f Celsius\r\n", t);
        PRINTF("Temperature: %u.%03u C\r\n",
               (unsigned int)(magnitude / 1000U),
               (unsigned int)(magnitude % 1000U));
    }
}


int main(void)
{

    BOARD_InitHardware();
    APP_TemperatureSensorTest();

    //vTaskDelay(pdMS_TO_TICKS(100U));


    vTaskStartScheduler();

    for (;;)
    {
    }
}
