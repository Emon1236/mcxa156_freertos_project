#include "app/app_position_task.h"
#include "app/app_encoder_task.h"
#include "app/app_motor_config.h"
#include "bsp/bsp_motor_pwm.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>

#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
#include "tools/position_tune/tune.h"
#endif

#define POSITION_SAMPLE_MAX_AGE_MS (3U * APP_ENCODER_PERIOD_MS)
#define POSITION_SAMPLE_TIMEOUT_MS 20U

//跨任务的参数请求；控制任务复制后才更新库上下文。
static app_position_params_t s_pendingParams = {
    APP_POSITION_KP, APP_POSITION_KD,
    APP_POSITION_UQ_LIMIT, APP_POSITION_RAMP
};
static foc_motor_t s_motor;
static app_position_state_t s_state = APP_POSITION_WAIT_SENSOR;
static bool s_enabled = false;
static float s_target = 0.0f;
static float s_speed = 0.0f;
static uint32_t s_start = 0U;

//这里只保存调度状态，算法历史保存在s_motor中。
typedef struct{
    bool running;
    uint32_t start;
    uint32_t sequence;
    TickType_t tick;
} position_run_t;

static void position_task(void *parameter);
static void position_update(position_run_t *run);

static void position_set_state(app_position_state_t state){
    taskENTER_CRITICAL();
    s_state = state;
    taskEXIT_CRITICAL();
}

static bool position_read(app_encoder_sample_t *sample){
    return APP_Encoder_GetLatest(sample) && sample->valid &&
           (TickType_t)(xTaskGetTickCount() - sample->timestampTicks) <=
               pdMS_TO_TICKS(POSITION_SAMPLE_TIMEOUT_MS);
}

static foc_feedback_t position_feedback(const app_encoder_sample_t *sample){
    foc_feedback_t feedback = {
        (float)sample->rawAngle * FOC_TWO_PI / 4096.0f,
        sample->filteredRpm * FOC_TWO_PI / 60.0f
    };
    return feedback;
}

static void position_zero(void){
    if (BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f) != kStatus_Success){
        (void)BSP_MotorPwm_Stop();
    }
}

static void position_fault(position_run_t *run, const app_encoder_sample_t *sample){
    position_zero();
    APP_Position_Stop();
    FOC_ResetControl(&s_motor);
    run->running = false;
    position_set_state(APP_POSITION_FAULT);
    PRINTF("Position fault: valid=%u, age=%u ticks.\r\n",
           (unsigned int)sample->valid,
           (unsigned int)(TickType_t)(xTaskGetTickCount() - sample->timestampTicks));
}

bool APP_Position_CreateTask(void){
    if (!FOC_Init(&s_motor, APP_MOTOR_BUS_VOLTAGE, APP_MOTOR_MAX_VOLTAGE,
                  APP_MOTOR_POLE_PAIRS, APP_MOTOR_SENSOR_DIRECTION) ||
        !FOC_SetPositionParams(&s_motor, &s_pendingParams)){
        return false;
    }
    return xTaskCreate(position_task, "position",
                       configMINIMAL_STACK_SIZE + 256U, NULL,
                       tskIDLE_PRIORITY + 3U, NULL) == pdPASS;
}

bool APP_Position_SetParams(const app_position_params_t *params){
    if (!FOC_PositionParamsValid(params, APP_MOTOR_MAX_VOLTAGE)){
        return false;
    }
    taskENTER_CRITICAL();
    s_pendingParams = *params;
    taskEXIT_CRITICAL();
    return true;
}

app_position_params_t APP_Position_GetParams(void){
    taskENTER_CRITICAL();
    app_position_params_t params = s_pendingParams;
    taskEXIT_CRITICAL();
    return params;
}

bool APP_Position_StartHold(void){
    return APP_Position_StartRotate(0.0f);
}

bool APP_Position_StartRotate(float speedRad){
    app_encoder_sample_t sample;
    if (!isfinite(speedRad) || !position_read(&sample)){
        return false;
    }
    foc_feedback_t feedback = position_feedback(&sample);
    float angle = FOC_MechanicalAngle(&s_motor, feedback.angle);

    taskENTER_CRITICAL();
    bool ready = s_state == APP_POSITION_READY || s_state == APP_POSITION_RUNNING;
    if (ready){
        s_target = angle;
        s_speed = speedRad;
        s_enabled = true;
        s_start++;
    }
    taskEXIT_CRITICAL();
    return ready;
}

bool APP_Position_SetTarget(float targetRad){
    if (!isfinite(targetRad)){
        return false;
    }
    float target = FOC_NormalizeAngle(targetRad);

    taskENTER_CRITICAL();
    bool ready = s_state == APP_POSITION_READY || s_state == APP_POSITION_RUNNING;
    if (ready){
        s_target = target;
        s_speed = 0.0f;
    }
    taskEXIT_CRITICAL();
    return ready;
}

void APP_Position_Stop(void){
    taskENTER_CRITICAL();
    s_enabled = false;
    s_speed = 0.0f;
    taskEXIT_CRITICAL();
}

app_position_state_t APP_Position_GetState(void){
    taskENTER_CRITICAL();
    app_position_state_t state = s_state;
    taskEXIT_CRITICAL();
    return state;
}

static void position_update(position_run_t *run){
    taskENTER_CRITICAL();
    bool enabled = s_enabled;
    float target = s_target;
    float speed = s_speed;
    uint32_t start = s_start;
    app_position_params_t params = s_pendingParams;
    taskEXIT_CRITICAL();

    if (!enabled){
        if (run->running){
            position_zero();
            FOC_ResetControl(&s_motor);
            run->running = false;
            position_set_state(APP_POSITION_READY);
        }
        return;
    }

    app_encoder_sample_t sample = {};
    if (!position_read(&sample) || !s_motor.calibrated){
        position_fault(run, &sample);
        return;
    }
    //短暂调度/总线延迟先归零输出；持续超过20ms才锁存故障。
    if ((TickType_t)(xTaskGetTickCount() - sample.timestampTicks) >
        pdMS_TO_TICKS(POSITION_SAMPLE_MAX_AGE_MS)){
        position_zero();
        return;
    }

    foc_feedback_t feedback = position_feedback(&sample);
    if (!run->running || run->start != start){
        FOC_PositionReset(&s_motor, &feedback, target);
        run->running = true;
        run->start = start;
        run->sequence = sample.sequence;
        run->tick = (TickType_t)sample.timestampTicks;
        position_zero();
        position_set_state(APP_POSITION_RUNNING);
        return;
    }
    if (sample.sequence == run->sequence){
        return;
    }

    float dt = (float)(TickType_t)(sample.timestampTicks - run->tick) /
               (float)configTICK_RATE_HZ;
    foc_pwm_t pwm;
    if (!FOC_SetPositionParams(&s_motor, &params) ||
        !FOC_PositionUpdate(&s_motor, &feedback, target, speed, dt, &pwm) ||
        BSP_MotorPwm_SetDuty(pwm.a, pwm.b, pwm.c) != kStatus_Success){
        position_fault(run, &sample);
        return;
    }

#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
    float angle = FOC_MechanicalAngle(&s_motor, feedback.angle);
    APP_Position_TuneSample(&sample,
        FOC_NormalizeAngle(angle + s_motor.positionState.error),
        s_motor.positionState.error, s_motor.positionState.lastOutput);
#endif

    run->sequence = sample.sequence;
    run->tick = (TickType_t)sample.timestampTicks;
}

static bool position_align(void){
    foc_pwm_t pwm;
    if (APP_MOTOR_ALIGN_VOLTAGE > s_motor.maxVoltage ||
        !FOC_Modulate(&s_motor, APP_MOTOR_ALIGN_VOLTAGE, 0.0f,
                      3.0f * FOC_PI / 2.0f, &pwm) ||
        BSP_MotorPwm_SetDuty(pwm.a, pwm.b, pwm.c) != kStatus_Success){
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(500U));
    app_encoder_sample_t sample = {};
    bool ready = position_read(&sample);
    if (ready){
        foc_feedback_t feedback = position_feedback(&sample);
        ready = FOC_SetZeroAngle(&s_motor, feedback.angle);
    }
    position_zero();
    vTaskDelay(pdMS_TO_TICKS(50U));
    return ready;
}

static void position_task(void *parameter){
    app_encoder_sample_t sample;
    position_run_t run = {};
    (void)parameter;

    //编码器任务独占AS5600初始化和读取，位置任务只消费快照。
    while (!position_read(&sample)){
        vTaskDelay(pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }

    position_set_state(APP_POSITION_ALIGNING);
    status_t status = BSP_MotorPwm_Init(APP_MOTOR_PWM_HZ);
    if (status == kStatus_Success){
        status = BSP_MotorPwm_SetDuty(0.5f, 0.5f, 0.5f);
    }
    if (status == kStatus_Success){
        status = BSP_MotorPwm_Start();
    }
    if (status != kStatus_Success || !position_align() || !position_read(&sample)){
        position_zero();
        position_set_state(APP_POSITION_FAULT);
        PRINTF("Position init failed.\r\n");
        vTaskDelete(NULL);
        return;
    }

    foc_feedback_t feedback = position_feedback(&sample);
    taskENTER_CRITICAL();
    s_target = FOC_MechanicalAngle(&s_motor, feedback.angle);
    s_state = APP_POSITION_READY;
    taskEXIT_CRITICAL();
#if APP_POSITION_AUTO_RUN && (!defined(APP_POSITION_TUNE) || !APP_POSITION_TUNE)
    while (!APP_Position_StartRotate(APP_POSITION_SPEED_RPS * FOC_TWO_PI)){
        vTaskDelay(pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }
    PRINTF("Position calibrated: zero=%d mrad, target speed=%d mRPM.\r\n",
           (int)(s_motor.zeroAngle * 1000.0f), (int)(APP_POSITION_SPEED_RPS * 60000.0f));
#else
    PRINTF("Position calibrated: zero=%d mrad, control OFF.\r\n",
           (int)(s_motor.zeroAngle * 1000.0f));
#endif

    TickType_t lastWakeTime = xTaskGetTickCount();
    for (;;){
#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
        APP_Position_TuneBefore();
#endif
        position_update(&run);
#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
        APP_Position_TuneAfter();
#endif
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }
}
