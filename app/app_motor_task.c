#include "app/app_motor_task.h"
#include "FOC/FOC.h"
#include "FOC/AS5600.h"
#include "fsl_debug_console.h"

#include <stdbool.h>


static int8_t s_motorDirection = DIRECTION_POSITIVE;
//静态函数声明区
static void foc_openloop_test_task(void *parameter);
static void foc_position_test_task(void *parameter);


bool Foc_openloop_Createdtask(void){
    BaseType_t result;
    result = xTaskCreate(foc_openloop_test_task,
                         "FOC OpenLoop",
                         configMINIMAL_STACK_SIZE + 256U,
                         NULL,
                         tskIDLE_PRIORITY + 3U,
                         NULL);
    return (result == pdPASS);
}

bool Foc_position_Createdtask(void){
    BaseType_t result;
    result = xTaskCreate(foc_position_test_task,
                         "FOC Position",
                         configMINIMAL_STACK_SIZE + 256U,
                         NULL,
                         tskIDLE_PRIORITY + 3U,
                         NULL);
    return (result == pdPASS);
}

void APP_Motor_SetDirection(int8_t direction){
    if((direction != DIRECTION_POSITIVE)&&(direction != DIRECTION_NEGATIVE)){
        return;
    }
    //短临界区，临界区内只复制变量，不执行其他复杂任务
    taskENTER_CRITICAL();
    s_motorDirection = direction;
    taskEXIT_CRITICAL();
}

int8_t APP_Motor_GetDirection(void){
    int8_t direction;

    taskENTER_CRITICAL();
    direction = s_motorDirection;
    taskEXIT_CRITICAL();

    return direction;
}

static void foc_openloop_test_task(void *parameter){
    TickType_t lastWakeTime;
    uint32_t printCounter;
    float targetMechanicalSpeed;
    float Uq;

    status_t status;
    float zeroAngle = 0.0f;

    (void)parameter;
    
    PRINTF("Motor task started.\r\n");

    status = AS5600_Init();
    PRINTF("AS5600_Init returned: %d\r\n", (int)status);

    if (status == kStatus_Success)
    {
        status = AS5600_CheckMagnet();
        PRINTF("AS5600_CheckMagnet returned: %d\r\n", (int)status);
    }

    if (status == kStatus_Success)
    {
        PRINTF("Align start.\r\n");

        status = FOC_AlignSensor(&zeroAngle);

        PRINTF("Align returned: %d\r\n", (int)status);
    }

    if (status != kStatus_Success)
    {
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);

        PRINTF("Motor stopped: sensor init or alignment failed.\r\n");

        /* 校准失败后不进入下面的开环运行。 */
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    PRINTF("zero_angle=%d mrad\r\n",(int)(zeroAngle * 1000.0f));

    /*
     * velocityopenloop()中的target单位是rad/s。
     * 60RPM = 1转/秒 = 2π rad/s。
     * 示例直接传入60，实际表示60rad/s，
     * 大约等于573RPM，而不是60RPM。
     */
    targetMechanicalSpeed = 6.28318530718f;

    lastWakeTime = xTaskGetTickCount();
    printCounter = 0U;

    for (;;){
        //这里修改方向
        Uq = velocityopenloop(targetMechanicalSpeed * (float)APP_Motor_GetDirection());
        /*
         * 每500次输出一次，避免PRINTF严重干扰5ms控制周期。
         */
        printCounter++;

        if (printCounter >= 500U){
            printCounter = 0U;

            PRINTF(
                "OpenLoop: Uq=%d mV, "
                "angle=%d mrad, "
                "duty=%u/%u/%u\r\n",
                (int)(Uq * 1000.0f),
                (int)(shaft_angle * 1000.0f),
                (unsigned int)(dc_a * 1000.0f),
                (unsigned int)(dc_b * 1000.0f),
                (unsigned int)(dc_c * 1000.0f));
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(5U));
    }
}

static void foc_position_test_task(void *parameter){
    TickType_t lastWakeTime;
    status_t status;

    float initialAngle;
    float targetAngle;
    float zeroAngle;
    
    (void)parameter;
    PRINTF("Position task started.\r\n");

    status = FOC_PositionInit(&initialAngle, &zeroAngle);

    if (status != kStatus_Success){
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);
        PRINTF("Position init failed: %d\r\n", (int)status);
        for (;;){
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }
    /* 第一次测试：保持初始化完成时的位置。 */
    //targetAngle = initialAngle;
    /* 第二次测试：相对当前位置转动30° */
    targetAngle = normalizeAngle(initialAngle + 0.5235987756f);
    
    PRINTF(
        "Position ready: initial=%d mrad, target=%d mrad, zero=%d mrad\r\n",
        (int)(initialAngle * 1000.0f),(int)(targetAngle * 1000.0f),(int)(zeroAngle * 1000.0f));
    
    lastWakeTime = xTaskGetTickCount();
    for (;;){
        status = FOC_PositionControl(targetAngle);
        if (status != kStatus_Success){
            SetPhaseVoltage(0.0f, 0.0f, 0.0f);
            PRINTF("Position control failed: %d\r\n", (int)status);
            for (;;){
                vTaskDelay(pdMS_TO_TICKS(1000U));
            }
        }
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(5U));
    }
}