#include "app/app_position_task.h"
#include "app/app_encoder_task.h"
#include "FOC/FOC.h"
#include "FOC/pid.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>

#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
#include "tools/position_tune/tune.h"
#endif

#define POSITION_PI 3.14159265359f
#define POSITION_SAMPLE_MAX_AGE_MS (3U * APP_ENCODER_PERIOD_MS)
//共享I3C总线等待可达10ms；短暂延迟先归零输出，持续超时才锁存故障。
#define POSITION_SAMPLE_TIMEOUT_MS 20U

static app_position_params_t s_params = {
    APP_POSITION_KP, APP_POSITION_KD,
    APP_POSITION_UQ_LIMIT, APP_POSITION_RAMP
};
static app_position_state_t s_state = APP_POSITION_WAIT_SENSOR;
static bool s_enabled = false;
static float s_target = 0.0f;
static float s_speed = 0.0f;  //机械rad/s
static uint32_t s_start = 0U;

//任务内部保存的上一份控制状态。
typedef struct{
    bool running;
    uint32_t start;
    uint32_t sequence;
    TickType_t tick;
    float angle;
    float error;
} position_run_t;

static void position_task(void *parameter);
static void position_update(PIDController &pid, position_run_t *run);

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

static float position_angle(uint16_t rawAngle){
    return normalizeAngle((float)FOC_GetSensorDirection() *
                          (float)rawAngle * 2.0f * POSITION_PI / 4096.0f);
}

static float position_delta(float angle){
    float delta = normalizeAngle(angle);
    return delta > POSITION_PI ? delta - 2.0f * POSITION_PI : delta;
}

bool APP_Position_CreateTask(void){
    return xTaskCreate(position_task, "position",
                       configMINIMAL_STACK_SIZE + 256U, NULL,
                       tskIDLE_PRIORITY + 3U, NULL) == pdPASS;
}

bool APP_Position_SetParams(const app_position_params_t *params){
    if (params == NULL ||
        !isfinite(params->kp) || !isfinite(params->kd) ||
        !isfinite(params->voltageLimit) || !isfinite(params->outputRamp) ||
        params->kp < 0.0f || params->kd < 0.0f ||
        params->voltageLimit <= 0.0f || params->voltageLimit > voltage_limit ||
        params->outputRamp < 0.0f){
        return false;
    }

    taskENTER_CRITICAL();
    s_params = *params;
    taskEXIT_CRITICAL();
    return true;
}

app_position_params_t APP_Position_GetParams(void){
    taskENTER_CRITICAL();
    app_position_params_t params = s_params;
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
    float angle = position_angle(sample.rawAngle);

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
    float target = normalizeAngle(targetRad);

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

static void position_update(PIDController &pid, position_run_t *run){
    taskENTER_CRITICAL();
    bool enabled = s_enabled;
    float target = s_target;
    float speed = s_speed;
    uint32_t start = s_start;
    app_position_params_t params = s_params;
    taskEXIT_CRITICAL();

    if (!enabled){
        if (run->running){
            SetPhaseVoltage(0.0f, 0.0f, 0.0f);
            pid.Reset();
            run->running = false;
            position_set_state(APP_POSITION_READY);
        }
        return;
    }

    app_encoder_sample_t sample = {};
    float angle;
    bool fresh = position_read(&sample);
    status_t status = fresh ? FOC_CalcElectricalAngle(sample.rawAngle, &angle) : kStatus_Fail;
    if (!fresh || status != kStatus_Success){
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);
        APP_Position_Stop();
        pid.Reset();
        run->running = false;
        position_set_state(APP_POSITION_FAULT);
        PRINTF("Position fault: valid=%u, age=%u ticks, angle status=%d.\r\n",
               (unsigned int)sample.valid,
               (unsigned int)(TickType_t)(xTaskGetTickCount() - sample.timestampTicks),
               (int)status);
        return;
    }

    if ((TickType_t)(xTaskGetTickCount() - sample.timestampTicks) >
        pdMS_TO_TICKS(POSITION_SAMPLE_MAX_AGE_MS)){
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);
        return;
    }

    //启动时先建立时间基准，不使用校准或停机期间的时间差。
    if (!run->running || run->start != start){
        pid.Reset();
        run->running = true;
        run->start = start;
        run->sequence = sample.sequence;
        run->tick = (TickType_t)sample.timestampTicks;
        run->angle = position_angle(sample.rawAngle);
        run->error = position_delta(target - run->angle);
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);
        position_set_state(APP_POSITION_RUNNING);
        return;
    }
    if (sample.sequence == run->sequence){
        return;
    }

    float dt = (float)(TickType_t)(sample.timestampTicks - run->tick) /
               (float)configTICK_RATE_HZ;
    float mechanicalAngle = position_angle(sample.rawAngle);
    if (speed != 0.0f){
        //累计目标与实际位移之差，跨圈或落后超过半圈时仍保持原方向。
        run->error += speed * dt - position_delta(mechanicalAngle - run->angle);
        target = normalizeAngle(mechanicalAngle + run->error);
    }
    else{
        run->error = position_delta(target - mechanicalAngle);
    }
    float error = run->error;
    float velocity = (float)FOC_GetSensorDirection() *
                     sample.filteredRpm * 2.0f * POSITION_PI / 60.0f;

    pid.P = params.kp;
    pid.I = 0.0f;
    pid.D = params.kd;
    pid.limit = params.voltageLimit;
    pid.output_ramp = params.outputRamp;
    //旋转时D对速度偏差提供阻尼；位置保持时speed为0，沿用原来的PD。
    float uq = pid.UpdatePD(error, velocity - speed, dt);
    SetPhaseVoltage(uq, 0.0f, angle);

#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
    APP_Position_TuneSample(&sample, target, error, uq);
#endif

    run->sequence = sample.sequence;
    run->tick = (TickType_t)sample.timestampTicks;
    run->angle = mechanicalAngle;
}

static void position_task(void *parameter){
    app_encoder_sample_t sample;
    float initialAngle;
    float zeroAngle;
    PIDController pid(0.0f, 0.0f, 0.0f, 0.0f, APP_POSITION_UQ_LIMIT);
    position_run_t run = {};

    (void)parameter;

    //编码器任务负责上电初始化；取得有效读数后再开始校准。
    while (!position_read(&sample)){
        vTaskDelay(pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }

    position_set_state(APP_POSITION_ALIGNING);
    status_t status = FOC_Init(APP_POSITION_BUS_VOLTAGE);
    if (status == kStatus_Success){
        status = FOC_PositionInit(&initialAngle, &zeroAngle);
        SetPhaseVoltage(0.0f, 0.0f, 0.0f);
    }
    if (status != kStatus_Success){
        position_set_state(APP_POSITION_FAULT);
        PRINTF("Position init failed: %d\r\n", (int)status);
        vTaskDelete(NULL);
        return;
    }

    float target = normalizeAngle((float)FOC_GetSensorDirection() * initialAngle);
    taskENTER_CRITICAL();
    s_target = target;
    s_state = APP_POSITION_READY;
    taskEXIT_CRITICAL();
#if APP_POSITION_AUTO_RUN && (!defined(APP_POSITION_TUNE) || !APP_POSITION_TUNE)
    //校准期间编码器也在运行，等到新鲜快照后以当前位置开始旋转。
    while (!APP_Position_StartRotate(APP_POSITION_SPEED_RPS * 2.0f * POSITION_PI)){
        vTaskDelay(pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }
    PRINTF("Position calibrated: zero=%d mrad, target speed=%d mRPM.\r\n",
           (int)(zeroAngle * 1000.0f), (int)(APP_POSITION_SPEED_RPS * 60000.0f));
#else
    PRINTF("Position calibrated: zero=%d mrad, control OFF.\r\n",
           (int)(zeroAngle * 1000.0f));
#endif

    TickType_t lastWakeTime = xTaskGetTickCount();
    for (;;){
#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
        APP_Position_TuneBefore();
#endif
        position_update(pid, &run);
#if defined(APP_POSITION_TUNE) && APP_POSITION_TUNE
        APP_Position_TuneAfter();
#endif
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));
    }
}
