# FOC 算法核心

`FOC.c`、`FOC.h` 使用标准 C11 和数学库，不依赖 FreeRTOS、NXP SDK、AS5600 或 PWM 驱动。它们包含开环速度目标推进、直接位置 PD、校准零偏计算、电角度计算和正弦 PWM 调制。

每个电机持有独立的 `foc_motor_t`。调用方应传入已初始化、有效的上下文和反馈指针，并保证同一个上下文只由一个控制执行流更新。算法返回占空比，不写硬件。

```c
foc_motor_t motor;
FOC_Init(&motor, 12.6f, 5.0f, 7U, 1);

foc_position_params_t params = {24.0f, 0.06f, 1.0f, 0.0f};
FOC_SetPositionParams(&motor, &params);
```

- `busVoltage` 是实际母线电压；`maxVoltage` 是最终 dq 矢量限幅。
- `position.voltageLimit` 是位置控制模式的 Uq 工作限幅，不替代最终限幅。
- `foc_feedback_t.angle` 为传感器机械角度 rad，`velocity` 为同一传感器方向的机械 rad/s。输入不预先乘 `direction`，库统一应用方向。
- 目标位置是库统一方向下的机械 rad；目标速度为机械 rad/s；`dt` 为实际秒数。
- `FOC_OpenLoopUpdate()` 的 `uq` 是显式驱动电压，不需要编码器或校准。
- `FOC_Modulate()` 可以在校准前使用；零输入返回三相 50% 占空比。

位置控制前，调用方先输出原有的固定校准磁场，等待后取得有效角度，再调用 `FOC_SetZeroAngle()`。沿用现有零偏约定，零偏为 `normalize(direction * polePairs * sensorAngle)`；硬件校准流程仍由 App 执行。

```c
FOC_SetZeroAngle(&motor, alignedSensorAngle);
FOC_PositionReset(&motor, &feedback, target);
FOC_PositionUpdate(&motor, &feedback, target, targetSpeed, dt, &pwm);
```

`targetSpeed=0` 时跟踪固定位置，走单圈最短路径；非0时累计目标与反馈位移之差，实现连续跨圈目标跟踪。当前是位置 PD 直接输出 Uq，没有增加速度 PI 或电流闭环。

启动或重新保持时调用 `FOC_PositionReset()`。停止时调用 `FOC_ResetControl()` 清除算法历史，校准结果仍保留。硬件零电压、PWM 停止和驱动器使能由 App/BSP 处理。

App 负责读取编码器、时间戳与样本序号、实际 dt、过期检测和故障策略。现有位置任务保持 3 ms 过期归零、20 ms 超时锁存故障，以及正常构建上电自动旋转的行为。公共硬件参数位于 `app/app_motor_config.h`，位置参数默认值位于 `app/app_position_task.h`。

旧 `foc_openloop.c` 和仅有旧实现调用的 `pid.cpp/.h` 已删除。开环和位置计算统一在 `FOC.c`；滤波仍由编码器任务调用。`AS5600.c/.h` 是现有的独立传感器驱动，不属于可移植的算法核心。

迁移时编译 `FOC.c` 并链接标准数学库，再接入平台自己的采样、调度和占空比写入接口。不要迁入 App 的 RTOS 时间戳或故障等待代码。

原生验证：

```text
python tests/foc/run_checks.py
```

该脚本需要 PATH 中的 gcc/g++，先以严格 C11 编译算法，再运行自动启动、手动启动和调参构建三种 App 行为检查。`tools/position_tune` 中的本地调参工具已适配新上下文；新零偏调试表达式为 `'app/app_position_task.cpp'::s_motor.zeroAngle`。
