#include "app/app_motor_task.h"
#include "app/app_motor_config.h"
#include "FOC/FOC.h"
#include "bsp/bsp_motor_pwm.h"
#include "fsl_debug_console.h"

#define OPENLOOP_PERIOD_MS 1U

static int8_t s_motorDirection = DIRECTION_POSITIVE;
static void foc_openloop_test_task(void *parameter);

bool Foc_openloop_Createdtask(void){
    return xTaskCreate(foc_openloop_test_task, "FOC OpenLoop",
                       configMINIMAL_STACK_SIZE + 256U, NULL,
                       tskIDLE_PRIORITY + 3U, NULL) == pdPASS;
}

void APP_Motor_SetDirection(int8_t direction){
    if (direction != DIRECTION_POSITIVE && direction != DIRECTION_NEGATIVE){
        return;
    }
    taskENTER_CRITICAL();
    s_motorDirection = direction;
    taskEXIT_CRITICAL();
}

int8_t APP_Motor_GetDirection(void){
    taskENTER_CRITICAL();
    int8_t direction = s_motorDirection;
    taskEXIT_CRITICAL();
    return direction;
}

static void foc_openloop_test_task(void *parameter){
    foc_motor_t motor;
    foc_pwm_t pwm;
    (void)parameter;

    //开环不需要编码器或电角度校准。
    bool ready = FOC_Init(&motor, APP_MOTOR_BUS_VOLTAGE, APP_MOTOR_MAX_VOLTAGE,
                          APP_MOTOR_POLE_PAIRS, APP_MOTOR_SENSOR_DIRECTION);
    if (!ready || BSP_MotorPwm_Init(APP_MOTOR_PWM_HZ) != kStatus_Success ||
        BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f) != kStatus_Success ||
        BSP_MotorPwm_Start() != kStatus_Success){
        (void)BSP_MotorPwm_Stop();
        PRINTF("OpenLoop init failed.\r\n");
        vTaskDelete(NULL);
        return;
    }

    TickType_t lastWakeTime = xTaskGetTickCount();
    TickType_t previousTick = lastWakeTime;
    uint32_t printCounter = 0U;
    for (;;){
        TickType_t now = xTaskGetTickCount();
        float dt = (float)(TickType_t)(now - previousTick) / (float)configTICK_RATE_HZ;
        if (dt > 0.0f){
            float speed = FOC_TWO_PI * (float)APP_Motor_GetDirection();
            if (!FOC_OpenLoopUpdate(&motor, speed, APP_MOTOR_MAX_VOLTAGE, dt, &pwm) ||
                BSP_MotorPwm_SetDuty(pwm.a, pwm.b, pwm.c) != kStatus_Success){
                (void)BSP_MotorPwm_Stop();
                vTaskDelete(NULL);
                return;
            }
            previousTick = now;
            if (++printCounter >= 1000U){
                printCounter = 0U;
                PRINTF("OpenLoop: angle=%d mrad, duty=%u/%u/%u\r\n",
                       (int)(motor.openAngle * 1000.0f),
                       (unsigned int)(pwm.a * 1000.0f),
                       (unsigned int)(pwm.b * 1000.0f),
                       (unsigned int)(pwm.c * 1000.0f));
            }
        }
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(OPENLOOP_PERIOD_MS));
    }
}
