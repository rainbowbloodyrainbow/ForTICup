#include "motor.h"

#include <stddef.h>

#define MOTOR_MAX_REVERSAL_DELAY_MS (0x7FFFFFFFU)

static bool Motor_ConfigIsValid(const Motor_Config *config)
{
    if (config == NULL) {
        return false;
    }

    if ((config->pwmTimer == NULL) ||
        (config->in1Port == NULL) ||
        (config->in2Port == NULL)) {
        return false;
    }

    if ((config->in1Pin == 0U) ||
        (config->in2Pin == 0U)) {
        return false;
    }

    if ((config->in1Port == config->in2Port) &&
        ((config->in1Pin & config->in2Pin) != 0U)) {
        return false;
    }

    if (config->maximumOutput > MOTOR_OUTPUT_MAX) {
        return false;
    }

    if (config->reversalDelayMs >
        MOTOR_MAX_REVERSAL_DELAY_MS) {
        return false;
    }

    return true;
}

static int16_t Motor_LimitOutput(
    const Motor *motor, int16_t output)
{
    int32_t limitedOutput;
    int32_t maximumOutput;

    limitedOutput = output;
    maximumOutput = motor->config.maximumOutput;

    if (limitedOutput > maximumOutput) {
        limitedOutput = maximumOutput;
    } else if (limitedOutput < -maximumOutput) {
        limitedOutput = -maximumOutput;
    }

    return (int16_t) limitedOutput;
}

static int8_t Motor_GetPhysicalDirection(
    const Motor *motor, int16_t output)
{
    int8_t direction;

    if (output > 0) {
        direction = 1;
    } else if (output < 0) {
        direction = -1;
    } else {
        direction = 0;
    }

    if (motor->config.inverted) {
        direction = (int8_t) -direction;
    }

    return direction;
}

static uint16_t Motor_GetMagnitude(int16_t output)
{
    int32_t magnitude;

    magnitude = output;
    if (magnitude < 0) {
        magnitude = -magnitude;
    }

    return (uint16_t) magnitude;
}

static void Motor_RecordZeroStart(
    Motor *motor, uint32_t nowMs)
{
    if (!motor->zeroOutputActive) {
        motor->zeroStartedMs = nowMs;
        motor->zeroOutputActive = true;
    }
}

static void Motor_ApplyBrakeHardware(
    Motor *motor, uint32_t nowMs)
{
    /*
     * 先令 PWM 为低，避免改变 IN1/IN2 时产生反向驱动脉冲。
     * 按 TB6612 真值表，PWM = 0 时输出进入短路制动。
     */
    PwmOutput_SetDuty(
        motor->config.pwmTimer,
        motor->config.pwmChannel,
        0U);

    DigitalOutput_High(
        motor->config.in1Port,
        motor->config.in1Pin);
    DigitalOutput_High(
        motor->config.in2Port,
        motor->config.in2Pin);

    motor->appliedOutput = 0;
    Motor_RecordZeroStart(motor, nowMs);
}

static void Motor_ApplyCoastHardware(
    Motor *motor, uint32_t nowMs)
{
    /*
     * 先经过 PWM = 0 的制动状态，再切换方向引脚，最后把 PWM 拉到 100%。
     * IN1 = IN2 = 0 且 PWM = 1 时，TB6612 单通道输出为高阻 Stop。
     */
    PwmOutput_SetDuty(
        motor->config.pwmTimer,
        motor->config.pwmChannel,
        0U);

    DigitalOutput_Low(
        motor->config.in1Port,
        motor->config.in1Pin);
    DigitalOutput_Low(
        motor->config.in2Port,
        motor->config.in2Pin);

    PwmOutput_SetDuty(
        motor->config.pwmTimer,
        motor->config.pwmChannel,
        MOTOR_OUTPUT_MAX);

    motor->appliedOutput = 0;
    Motor_RecordZeroStart(motor, nowMs);
}

static void Motor_ApplyDrive(
    Motor *motor, int16_t output)
{
    int8_t physicalDirection;
    uint16_t magnitude;

    physicalDirection =
        Motor_GetPhysicalDirection(motor, output);
    magnitude = Motor_GetMagnitude(output);

    /*
     * Coast 模式下 PWM 为 100%，所以从零输出状态重新起动时必须先把 PWM
     * 拉低，再改变 IN1/IN2，避免方向引脚先变化而产生一个全输出脉冲。
     *
     * 同方向调速时 zeroOutputActive 为 false，不插入额外的零占空比，避免
     * 每次控制更新都人为制造 PWM 缺口。
     */
    if (motor->zeroOutputActive) {
        PwmOutput_SetDuty(
            motor->config.pwmTimer,
            motor->config.pwmChannel,
            0U);
    }

    if (physicalDirection > 0) {
        DigitalOutput_High(
            motor->config.in1Port,
            motor->config.in1Pin);
        DigitalOutput_Low(
            motor->config.in2Port,
            motor->config.in2Pin);
    } else {
        DigitalOutput_Low(
            motor->config.in1Port,
            motor->config.in1Pin);
        DigitalOutput_High(
            motor->config.in2Port,
            motor->config.in2Pin);
    }

    PwmOutput_SetDuty(
        motor->config.pwmTimer,
        motor->config.pwmChannel,
        magnitude);

    motor->appliedOutput = output;
    motor->lastPhysicalDirection = physicalDirection;
    motor->zeroOutputActive = false;

    if (output > 0) {
        motor->mode = MOTOR_MODE_FORWARD;
    } else {
        motor->mode = MOTOR_MODE_REVERSE;
    }
}

static bool Motor_ReversalDelayHasElapsed(
    const Motor *motor, uint32_t nowMs)
{
    uint32_t elapsedMs;

    elapsedMs = nowMs - motor->zeroStartedMs;

    return elapsedMs >= motor->config.reversalDelayMs;
}

bool Motor_Init(
    Motor *motor, const Motor_Config *config, uint32_t nowMs)
{
    if ((motor == NULL) ||
        !Motor_ConfigIsValid(config)) {
        return false;
    }

    motor->config = *config;
    motor->requestedOutput = 0;
    motor->appliedOutput = 0;
    motor->pendingOutput = 0;
    motor->zeroStartedMs = nowMs;
    motor->lastPhysicalDirection = 0;
    motor->mode = MOTOR_MODE_BRAKE;
    motor->initialized = true;
    motor->zeroOutputActive = false;
    motor->reversalPending = false;

    Motor_ApplyBrakeHardware(motor, nowMs);

    return true;
}

void Motor_SetOutput(
    Motor *motor, int16_t output, uint32_t nowMs)
{
    int16_t limitedOutput;
    int8_t physicalDirection;

    if ((motor == NULL) || !motor->initialized) {
        return;
    }

    limitedOutput = Motor_LimitOutput(motor, output);
    motor->requestedOutput = limitedOutput;

    if (limitedOutput == 0) {
        motor->pendingOutput = 0;
        motor->reversalPending = false;
        Motor_ApplyBrakeHardware(motor, nowMs);
        motor->mode = MOTOR_MODE_BRAKE;
        return;
    }

    physicalDirection =
        Motor_GetPhysicalDirection(motor, limitedOutput);

    if ((motor->lastPhysicalDirection == 0) ||
        (physicalDirection ==
            motor->lastPhysicalDirection)) {
        motor->pendingOutput = 0;
        motor->reversalPending = false;
        Motor_ApplyDrive(motor, limitedOutput);
        return;
    }

    if (!motor->zeroOutputActive ||
        (motor->mode == MOTOR_MODE_COAST)) {
        Motor_ApplyBrakeHardware(motor, nowMs);
    }

    motor->pendingOutput = limitedOutput;
    motor->reversalPending = true;
    motor->mode = MOTOR_MODE_REVERSAL_WAIT;

    if (Motor_ReversalDelayHasElapsed(motor, nowMs)) {
        Motor_ApplyDrive(motor, motor->pendingOutput);
        motor->pendingOutput = 0;
        motor->reversalPending = false;
    }
}

void Motor_Process(Motor *motor, uint32_t nowMs)
{
    if ((motor == NULL) ||
        !motor->initialized ||
        !motor->reversalPending) {
        return;
    }

    if (!Motor_ReversalDelayHasElapsed(motor, nowMs)) {
        return;
    }

    Motor_ApplyDrive(motor, motor->pendingOutput);
    motor->pendingOutput = 0;
    motor->reversalPending = false;
}

void Motor_Forward(
    Motor *motor, uint16_t output, uint32_t nowMs)
{
    if (output > MOTOR_OUTPUT_MAX) {
        output = MOTOR_OUTPUT_MAX;
    }

    Motor_SetOutput(motor, (int16_t) output, nowMs);
}

void Motor_Reverse(
    Motor *motor, uint16_t output, uint32_t nowMs)
{
    if (output > MOTOR_OUTPUT_MAX) {
        output = MOTOR_OUTPUT_MAX;
    }

    Motor_SetOutput(
        motor, (int16_t) -(int32_t) output, nowMs);
}

void Motor_Coast(Motor *motor, uint32_t nowMs)
{
    if ((motor == NULL) || !motor->initialized) {
        return;
    }

    motor->requestedOutput = 0;
    motor->pendingOutput = 0;
    motor->reversalPending = false;

    Motor_ApplyCoastHardware(motor, nowMs);
    motor->mode = MOTOR_MODE_COAST;
}

void Motor_Brake(Motor *motor, uint32_t nowMs)
{
    if ((motor == NULL) || !motor->initialized) {
        return;
    }

    motor->requestedOutput = 0;
    motor->pendingOutput = 0;
    motor->reversalPending = false;

    Motor_ApplyBrakeHardware(motor, nowMs);
    motor->mode = MOTOR_MODE_BRAKE;
}

bool Motor_IsInitialized(const Motor *motor)
{
    return (motor != NULL) && motor->initialized;
}

bool Motor_IsReversalPending(const Motor *motor)
{
    return (motor != NULL) &&
        motor->initialized &&
        motor->reversalPending;
}

int16_t Motor_GetRequestedOutput(const Motor *motor)
{
    if ((motor == NULL) || !motor->initialized) {
        return 0;
    }

    return motor->requestedOutput;
}

int16_t Motor_GetAppliedOutput(const Motor *motor)
{
    if ((motor == NULL) || !motor->initialized) {
        return 0;
    }

    return motor->appliedOutput;
}

Motor_Mode Motor_GetMode(const Motor *motor)
{
    if ((motor == NULL) || !motor->initialized) {
        return MOTOR_MODE_UNINITIALIZED;
    }

    return motor->mode;
}

bool Motor_StandbyInit(
    Motor_Standby *standby, GPIO_Regs *port, uint32_t pin)
{
    if ((standby == NULL) ||
        (port == NULL) ||
        (pin == 0U)) {
        return false;
    }

    standby->port = port;
    standby->pin = pin;
    standby->initialized = true;
    standby->enabled = false;

    DigitalOutput_Low(standby->port, standby->pin);

    return true;
}

void Motor_StandbyEnable(Motor_Standby *standby)
{
    if ((standby == NULL) || !standby->initialized) {
        return;
    }

    DigitalOutput_High(standby->port, standby->pin);
    standby->enabled = true;
}

void Motor_StandbyDisable(Motor_Standby *standby)
{
    if ((standby == NULL) || !standby->initialized) {
        return;
    }

    DigitalOutput_Low(standby->port, standby->pin);
    standby->enabled = false;
}

bool Motor_StandbyIsEnabled(const Motor_Standby *standby)
{
    return (standby != NULL) &&
        standby->initialized &&
        standby->enabled;
}
