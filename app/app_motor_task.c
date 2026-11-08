#include "app/app_motor_task.h"
#include "app/app_motor_config.h"
#include "app/app_encoder_task.h"
#include "FOC/FOC.h"
#include "bsp/bsp_motor_pwm.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"

#define MOTOR_PERIOD_MS          1U
#define MOTOR_SENSOR_TIMEOUT_MS  20U
#define MOTOR_ALIGN_MS           500U
#define MOTOR_SETTLE_MS          50U

typedef struct{
    app_motor_mode_t mode;
    int8_t direction;
    uint16_t rpm;
} motor_command_t;

typedef enum{
    MOTOR_STOPPED = 0U,
    MOTOR_ALIGNING,
    MOTOR_SETTLING,
    MOTOR_RUNNING
} motor_phase_t;
/* UI 任务写入，电机任务读取。 */
static motor_command_t s_command = {
    APP_MOTOR_OFF, DIRECTION_POSITIVE, 0U
};

/* 以下状态只允许电机任务修改。 */
static foc_motor_t s_motor;
static app_motor_mode_t s_mode = APP_MOTOR_OFF;
static motor_phase_t s_phase = MOTOR_STOPPED;

static bool s_pwmReady = false;
static bool s_fault = false;

static TickType_t s_phaseTick;
static TickType_t s_lastTick;
static uint32_t s_sequence;
static float s_target;

static void motor_task(void *parameter);

void APP_Motor_SetControl(app_motor_mode_t mode,
                         int8_t direction,
                         uint16_t rpm){
    taskENTER_CRITICAL();
    s_command.mode = mode;
    s_command.direction = direction;
    s_command.rpm = rpm;
    taskEXIT_CRITICAL();
}

static motor_command_t motor_get_command(void){
    motor_command_t command;

    taskENTER_CRITICAL();
    command = s_command;
    taskEXIT_CRITICAL();

    return command;
}

static bool motor_read_encoder(app_encoder_sample_t *sample){
    return APP_Encoder_GetLatest(sample) &&
           sample->valid &&
           (TickType_t)(xTaskGetTickCount() -
                        sample->timestampTicks) <=
               pdMS_TO_TICKS(MOTOR_SENSOR_TIMEOUT_MS);
}

static foc_feedback_t motor_feedback(const app_encoder_sample_t *sample){
    foc_feedback_t feedback;

    feedback.angle =
        (float)sample->rawAngle * FOC_TWO_PI / 4096.0f;

    feedback.velocity =
        sample->filteredRpm * FOC_TWO_PI / 60.0f;

    return feedback;
}

static bool motor_write(const foc_pwm_t *pwm){
    return BSP_MotorPwm_SetDuty(pwm->a, pwm->b, pwm->c) ==
           kStatus_Success;
}

static void motor_release(void){
    BSP_MotorPwm_Deinit();

    FOC_ResetControl(&s_motor);

    s_pwmReady = false;
    s_phase = MOTOR_STOPPED;
}

/* 故障后保持停止，退出弹窗再进入才能重新启动。 */
static void motor_fault(void){
    motor_release();
    s_fault = true;

    PRINTF("Motor stopped: control fault.\r\n");
}

static bool motor_start_pwm(void){
    if (BSP_MotorPwm_Init(APP_MOTOR_PWM_HZ) != kStatus_Success){
        return false;
    }

    if (BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f) !=
        kStatus_Success){
        return false;
    }

    if (BSP_MotorPwm_Start() != kStatus_Success){
        return false;
    }

    s_pwmReady = true;
    return true;
}

static bool motor_position_reset(const app_encoder_sample_t *sample){
    foc_feedback_t feedback = motor_feedback(sample);

    s_target = FOC_MechanicalAngle(&s_motor, feedback.angle);

    if (!FOC_PositionReset(&s_motor, &feedback, s_target)){
        return false;
    }

    s_sequence = sample->sequence;
    s_lastTick = (TickType_t)sample->timestampTicks;
    s_phase = MOTOR_RUNNING;

    return true;
}

static void motor_update(const motor_command_t *command){
    TickType_t now = xTaskGetTickCount();
    app_encoder_sample_t sample = {0};
    foc_pwm_t pwm;

    /* 切换模式前，先释放上一种模式占用的 PWM。 */
    if (command->mode != s_mode){
        motor_release();

        s_mode = command->mode;
        s_fault = false;
    }

    if (s_mode == APP_MOTOR_OFF || s_fault){
        return;
    }

    /* 0 RPM 采用停止驱动，不进入位置保持。 */
    if (s_mode == APP_MOTOR_POSITION && command->rpm == 0U){
        if (s_pwmReady){
            motor_release();
        }
        return;
    }

    if (s_mode == APP_MOTOR_POSITION){
        if (!motor_read_encoder(&sample)){
            if (s_pwmReady){
                motor_fault();
            }
            /* 尚未启动时等待编码器，不输出 PWM。 */
            return;
        }
    }

    if (!s_pwmReady){
        if (!motor_start_pwm()){
            motor_fault();
            return;
        }

        FOC_ResetControl(&s_motor);

        if (s_mode == APP_MOTOR_OPENLOOP){
            s_lastTick = now;
            s_phase = MOTOR_RUNNING;
            return;
        }

        /* 首次闭环启动才校准，后续保留电角度零偏。 */
        if (!s_motor.calibrated){
            if (!FOC_Modulate(&s_motor,
                              APP_MOTOR_ALIGN_VOLTAGE,
                              0.0f,
                              3.0f * FOC_PI / 2.0f,
                              &pwm) ||
                !motor_write(&pwm)){
                motor_fault();
                return;
            }

            s_phase = MOTOR_ALIGNING;
            s_phaseTick = now;
            return;
        }

        if (!motor_position_reset(&sample)){
            motor_fault();
        }
        return;
    }

    if (s_mode == APP_MOTOR_OPENLOOP){
        TickType_t elapsed = (TickType_t)(now - s_lastTick);

        if (elapsed == 0U){
            return;
        }

        /* 调度间隔过长时停止，避免开环角度大幅跳变。 */
        if (elapsed > pdMS_TO_TICKS(20U)){
            motor_fault();
            return;
        }

        float dt = (float)elapsed / (float)configTICK_RATE_HZ;
        float speed = APP_MOTOR_OPENLOOP_RPM *
                      FOC_TWO_PI / 60.0f *
                      (float)command->direction;

        if (!FOC_OpenLoopUpdate(&s_motor,
                                speed,
                                APP_MOTOR_OPENLOOP_UQ,
                                dt,
                                &pwm) ||
            !motor_write(&pwm)){
            motor_fault();
            return;
        }

        s_lastTick = now;
        return;
    }

    if (s_phase == MOTOR_ALIGNING){
        if ((TickType_t)(now - s_phaseTick) <
            pdMS_TO_TICKS(MOTOR_ALIGN_MS)){
            return;
        }

        foc_feedback_t feedback = motor_feedback(&sample);

        if (!FOC_SetZeroAngle(&s_motor, feedback.angle) ||
            BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f) !=
                kStatus_Success){
            motor_fault();
            return;
        }

        s_phase = MOTOR_SETTLING;
        s_phaseTick = now;
        return;
    }

    if (s_phase == MOTOR_SETTLING){
        if ((TickType_t)(now - s_phaseTick) <
            pdMS_TO_TICKS(MOTOR_SETTLE_MS)){
            return;
        }

        if (!motor_position_reset(&sample)){
            motor_fault();
        }
        return;
    }

    /* 相同的编码器快照不能重复参与位置计算。 */
    if (sample.sequence == s_sequence){
        return;
    }

    TickType_t elapsed =
        (TickType_t)((TickType_t)sample.timestampTicks - s_lastTick);

    if (elapsed == 0U){
        return;
    }

    float dt = (float)elapsed / (float)configTICK_RATE_HZ;
    float speed = (float)command->rpm * FOC_TWO_PI / 60.0f;

    foc_feedback_t feedback = motor_feedback(&sample);

    if (!FOC_PositionUpdate(&s_motor,
                            &feedback,
                            s_target,
                            speed,
                            dt,
                            &pwm) ||
        !motor_write(&pwm)){
        motor_fault();
        return;
    }

    s_sequence = sample.sequence;
    s_lastTick = (TickType_t)sample.timestampTicks;
}

bool APP_Motor_CreateTask(void){
    const foc_position_params_t params = {
        APP_POSITION_KP,
        APP_POSITION_KD,
        APP_POSITION_UQ_LIMIT,
        APP_POSITION_RAMP
    };

    if (!FOC_Init(&s_motor,
                   APP_MOTOR_BUS_VOLTAGE,
                   APP_MOTOR_MAX_VOLTAGE,
                   APP_MOTOR_POLE_PAIRS,
                   APP_MOTOR_SENSOR_DIRECTION) ||
        !FOC_SetPositionParams(&s_motor, &params)){
        return false;
    }

    return xTaskCreate(motor_task,
                       "motor",
                       configMINIMAL_STACK_SIZE + 256U,
                       NULL,
                       tskIDLE_PRIORITY + 3U,
                       NULL) == pdPASS;
}

static void motor_task(void *parameter){
    (void)parameter;

    /* 上电先将电机 PWM 引脚置为停止状态。 */
    motor_release();

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;){
        motor_command_t command = motor_get_command();

        motor_update(&command);

        vTaskDelayUntil(&lastWakeTime,
                        pdMS_TO_TICKS(MOTOR_PERIOD_MS));
    }
}