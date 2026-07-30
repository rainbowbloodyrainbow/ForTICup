#include "chassis.h"

#include <stddef.h>

static bool Chassis_IsConfigValid(
    const Chassis_Config *config)
{
    if ((config == NULL) ||
        (config->leftMotor == NULL) ||
        (config->rightMotor == NULL) ||
        (config->leftMotor == config->rightMotor) ||
        (config->standby == NULL)) {
        return false;
    }

    return Motor_IsInitialized(config->leftMotor) &&
        Motor_IsInitialized(config->rightMotor) &&
        config->standby->initialized &&
        (config->maximumDriveOutput <= MOTOR_OUTPUT_MAX) &&
        (config->maximumTurnOutput <=
            config->maximumDriveOutput) &&
        (config->leftOpenLoopScalePermille <= 1000U) &&
        (config->rightOpenLoopScalePermille <= 1000U);
}

static int16_t Chassis_ClampForwardOutput(
    const Chassis *chassis, int32_t output)
{
    int32_t maximum = chassis->config.maximumDriveOutput;

    if (output < 0) {
        output = 0;
    } else if (output > maximum) {
        output = maximum;
    }
    return (int16_t) output;
}

static int16_t Chassis_ClampTurn(
    const Chassis *chassis, int32_t output)
{
    int32_t maximum = chassis->config.maximumTurnOutput;

    if (output < -maximum) {
        output = -maximum;
    } else if (output > maximum) {
        output = maximum;
    }
    return (int16_t) output;
}

static int16_t Chassis_ApplyOpenLoopScale(
    int16_t output, uint16_t scalePermille)
{
    return (int16_t)
        (((int32_t) output * scalePermille) / 1000);
}

static void Chassis_ApplyWheelOutputs(
    Chassis *chassis,
    int32_t leftOutput,
    int32_t rightOutput,
    uint32_t nowMs)
{
    int16_t limitedLeft;
    int16_t limitedRight;
    int16_t scaledLeft;
    int16_t scaledRight;

    limitedLeft =
        Chassis_ClampForwardOutput(chassis, leftOutput);
    limitedRight =
        Chassis_ClampForwardOutput(chassis, rightOutput);
    scaledLeft = Chassis_ApplyOpenLoopScale(
        limitedLeft,
        chassis->config.leftOpenLoopScalePermille);
    scaledRight = Chassis_ApplyOpenLoopScale(
        limitedRight,
        chassis->config.rightOpenLoopScalePermille);

    Motor_SetOutput(
        chassis->config.leftMotor, scaledLeft, nowMs);
    Motor_SetOutput(
        chassis->config.rightMotor, scaledRight, nowMs);
    chassis->leftOutput = scaledLeft;
    chassis->rightOutput = scaledRight;
}

bool Chassis_Init(
    Chassis *chassis, const Chassis_Config *config)
{
    if ((chassis == NULL) ||
        !Chassis_IsConfigValid(config)) {
        return false;
    }

    chassis->config = *config;
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
    chassis->enabled = false;
    chassis->initialized = true;
    return true;
}

bool Chassis_Enable(
    Chassis *chassis, uint32_t nowMs)
{
    if ((chassis == NULL) ||
        !chassis->initialized) {
        return false;
    }

    Motor_Brake(chassis->config.leftMotor, nowMs);
    Motor_Brake(chassis->config.rightMotor, nowMs);

    PwmOutput_Start(
        chassis->config.leftMotor->config.pwmTimer);
    if (chassis->config.rightMotor->config.pwmTimer !=
        chassis->config.leftMotor->config.pwmTimer) {
        PwmOutput_Start(
            chassis->config.rightMotor->config.pwmTimer);
    }

    Motor_StandbyEnable(chassis->config.standby);
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
    chassis->enabled =
        Motor_StandbyIsEnabled(chassis->config.standby);
    return chassis->enabled;
}

void Chassis_Disable(
    Chassis *chassis, uint32_t nowMs)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return;
    }

    Motor_Brake(chassis->config.leftMotor, nowMs);
    Motor_Brake(chassis->config.rightMotor, nowMs);
    Motor_StandbyDisable(chassis->config.standby);
    PwmOutput_Stop(
        chassis->config.leftMotor->config.pwmTimer);
    if (chassis->config.rightMotor->config.pwmTimer !=
        chassis->config.leftMotor->config.pwmTimer) {
        PwmOutput_Stop(
            chassis->config.rightMotor->config.pwmTimer);
    }
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
    chassis->enabled = false;
}

void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t turnOutput,
    uint32_t nowMs)
{
    int16_t limitedDrive;
    int16_t limitedTurn;

    if ((chassis == NULL) ||
        !chassis->initialized ||
        !chassis->enabled) {
        return;
    }

    limitedDrive =
        Chassis_ClampForwardOutput(chassis, driveOutput);
    limitedTurn =
        Chassis_ClampTurn(chassis, turnOutput);
    Chassis_ApplyWheelOutputs(
        chassis,
        (int32_t) limitedDrive - limitedTurn,
        (int32_t) limitedDrive + limitedTurn,
        nowMs);
    chassis->driveOutput = limitedDrive;
    chassis->turnOutput = limitedTurn;
}

void Chassis_SetWheelOutputs(
    Chassis *chassis,
    int16_t leftOutput,
    int16_t rightOutput,
    uint32_t nowMs)
{
    int16_t limitedLeft;
    int16_t limitedRight;

    if ((chassis == NULL) ||
        !chassis->initialized ||
        !chassis->enabled) {
        return;
    }

    limitedLeft =
        Chassis_ClampForwardOutput(chassis, leftOutput);
    limitedRight =
        Chassis_ClampForwardOutput(chassis, rightOutput);
    Chassis_ApplyWheelOutputs(
        chassis, limitedLeft, limitedRight, nowMs);
    chassis->driveOutput =
        (int16_t) (((int32_t) limitedLeft +
            limitedRight) / 2);
    chassis->turnOutput =
        (int16_t) (((int32_t) limitedRight -
            limitedLeft) / 2);
}

void Chassis_Brake(
    Chassis *chassis, uint32_t nowMs)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return;
    }

    Motor_Brake(chassis->config.leftMotor, nowMs);
    Motor_Brake(chassis->config.rightMotor, nowMs);
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
}

void Chassis_Coast(
    Chassis *chassis, uint32_t nowMs)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return;
    }

    Motor_Coast(chassis->config.leftMotor, nowMs);
    Motor_Coast(chassis->config.rightMotor, nowMs);
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
}

void Chassis_Process(
    Chassis *chassis, uint32_t nowMs)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return;
    }

    Motor_Process(chassis->config.leftMotor, nowMs);
    Motor_Process(chassis->config.rightMotor, nowMs);
}
