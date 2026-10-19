#include "app/app_motor_task.h"
#include "FOC/FOC.h"
#include "fsl_debug_console.h"

#include <stdbool.h>


static int8_t s_motorDirection = DIRECTION_POSITIVE;
//静态函数声明区
static void foc_openloop_test_task(void *parameter);

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

static void foc_openloop_test_task(void *parameter){
    TickType_t lastWakeTime;
    uint32_t printCounter;
    float targetMechanicalSpeed;
    float Uq;

    (void)parameter;

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