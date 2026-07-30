#include "chassis.h"

#include <stddef.h>

static bool Chassis_IsConfigValid(
    const Chassis_Config *config)
{
    if ((config == NULL) ||
        (config->leftMotor == NULL) ||
        (config->rightMotor == NULL) ||
        (config->leftMotor == config->rightMotor)) {
        return false;
    }

    return Motor_IsInitialized(config->leftMotor) &&
        Motor_IsInitialized(config->rightMotor) &&
        ((config->standby == NULL) ||
            config->standby->initialized) &&
        (config->maximumDriveOutput <= MOTOR_OUTPUT_MAX) &&
        (config->maximumTurnOutput <=
            config->maximumDriveOutput) &&
        (config->leftOpenLoopScalePermille <= 1000U) &&
        (config->rightOpenLoopScalePermille <= 1000U);
}

static int16_t Chassis_ClampDrive(
    const Chassis *chassis, int32_t output)
{
    int32_t maximum = chassis->config.maximumDriveOutput;

    if (output < -maximum) {
        output = -maximum;
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

static int32_t Chassis_Absolute(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void Chassis_NormalizeWheelOutputs(
    const Chassis *chassis,
    int32_t *leftOutput,
    int32_t *rightOutput)
{
    int32_t leftMagnitude;
    int32_t rightMagnitude;
    int32_t maximumMagnitude;
    int32_t maximumOutput;

    leftMagnitude = Chassis_Absolute(*leftOutput);
    rightMagnitude = Chassis_Absolute(*rightOutput);
    maximumMagnitude =
        (leftMagnitude > rightMagnitude) ?
        leftMagnitude : rightMagnitude;
    maximumOutput = chassis->config.maximumDriveOutput;

    if ((maximumMagnitude > maximumOutput) &&
        (maximumMagnitude != 0)) {
        *leftOutput =
            (*leftOutput * maximumOutput) /
            maximumMagnitude;
        *rightOutput =
            (*rightOutput * maximumOutput) /
            maximumMagnitude;
    }
}

static void Chassis_ApplyWheelOutputs(
    Chassis *chassis,
    int32_t leftOutput,
    int32_t rightOutput,
    uint32_t nowMs)
{
    int16_t scaledLeft;
    int16_t scaledRight;

    Chassis_NormalizeWheelOutputs(
        chassis, &leftOutput, &rightOutput);
    scaledLeft = Chassis_ApplyOpenLoopScale(
        (int16_t) leftOutput,
        chassis->config.leftOpenLoopScalePermille);
    scaledRight = Chassis_ApplyOpenLoopScale(
        (int16_t) rightOutput,
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

    if (chassis->config.standby != NULL) {
        Motor_StandbyEnable(chassis->config.standby);
    }
    chassis->driveOutput = 0;
    chassis->turnOutput = 0;
    chassis->leftOutput = 0;
    chassis->rightOutput = 0;
    chassis->enabled =
        (chassis->config.standby == NULL) ||
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
    if (chassis->config.standby != NULL) {
        Motor_StandbyDisable(chassis->config.standby);
    }
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
        Chassis_ClampDrive(chassis, driveOutput);
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
    int32_t normalizedLeft;
    int32_t normalizedRight;

    if ((chassis == NULL) ||
        !chassis->initialized ||
        !chassis->enabled) {
        return;
    }

    normalizedLeft = leftOutput;
    normalizedRight = rightOutput;
    Chassis_NormalizeWheelOutputs(
        chassis, &normalizedLeft, &normalizedRight);
    Chassis_ApplyWheelOutputs(
        chassis, normalizedLeft, normalizedRight, nowMs);
    chassis->driveOutput =
        (int16_t) ((normalizedLeft +
            normalizedRight) / 2);
    chassis->turnOutput =
        (int16_t) ((normalizedRight -
            normalizedLeft) / 2);
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

int16_t Chassis_GetDriveOutput(const Chassis *chassis)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return 0;
    }
    return chassis->driveOutput;
}

int16_t Chassis_GetTurnOutput(const Chassis *chassis)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return 0;
    }
    return chassis->turnOutput;
}

int16_t Chassis_GetLeftOutput(const Chassis *chassis)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return 0;
    }
    return chassis->leftOutput;
}

int16_t Chassis_GetRightOutput(const Chassis *chassis)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return 0;
    }
    return chassis->rightOutput;
}
