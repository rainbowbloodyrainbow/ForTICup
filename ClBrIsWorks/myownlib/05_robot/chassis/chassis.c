#include "chassis.h"

#include <stddef.h>

static bool Chassis_IsConfigValid(
    const Chassis_Config *config)
{
    if ((config == NULL) ||
        (config->leftMotor == NULL) ||
        (config->rightMotor == NULL) ||
        (config->leftMotor == config->rightMotor) ||
        (config->standby == NULL) ||
        (config->steeringServo == NULL)) {
        return false;
    }

    return Motor_IsInitialized(config->leftMotor) &&
        Motor_IsInitialized(config->rightMotor) &&
        config->standby->initialized &&
        config->steeringServo->initialized &&
        (config->maximumDriveOutput <= MOTOR_OUTPUT_MAX) &&
        (config->maximumSteeringCommand <=
            SERVO_COMMAND_MAX) &&
        (config->leftOpenLoopScalePermille <= 1000U) &&
        (config->rightOpenLoopScalePermille <= 1000U);
}

static int16_t Chassis_ClampDrive(
    const Chassis *chassis, int32_t output)
{
    int32_t maximum = chassis->config.maximumDriveOutput;

    if (output > maximum) {
        output = maximum;
    } else if (output < -maximum) {
        output = -maximum;
    }
    return (int16_t) output;
}

static int16_t Chassis_ClampSteering(
    const Chassis *chassis, int32_t command)
{
    int32_t maximum =
        chassis->config.maximumSteeringCommand;

    if (command > maximum) {
        command = maximum;
    } else if (command < -maximum) {
        command = -maximum;
    }
    return (int16_t) command;
}

static int16_t Chassis_ApplyOpenLoopScale(
    int16_t output, uint16_t scalePermille)
{
    int32_t scaled;

    scaled =
        ((int32_t) output * scalePermille) / 1000;
    return (int16_t) scaled;
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
    chassis->steeringCommand = 0;
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
    if (!Servo_Center(chassis->config.steeringServo) ||
        !Servo_Enable(chassis->config.steeringServo)) {
        return false;
    }

    PwmOutput_Start(
        chassis->config.leftMotor->config.pwmTimer);
    if (chassis->config.rightMotor->config.pwmTimer !=
        chassis->config.leftMotor->config.pwmTimer) {
        PwmOutput_Start(
            chassis->config.rightMotor->config.pwmTimer);
    }

    Motor_StandbyEnable(chassis->config.standby);
    chassis->driveOutput = 0;
    chassis->steeringCommand = 0;
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
    Chassis_CenterSteering(chassis);
    Motor_StandbyDisable(chassis->config.standby);
    PwmOutput_Stop(
        chassis->config.leftMotor->config.pwmTimer);
    if (chassis->config.rightMotor->config.pwmTimer !=
        chassis->config.leftMotor->config.pwmTimer) {
        PwmOutput_Stop(
            chassis->config.rightMotor->config.pwmTimer);
    }
    Servo_Disable(chassis->config.steeringServo);
    chassis->driveOutput = 0;
    chassis->steeringCommand = 0;
    chassis->enabled = false;
}

void Chassis_SetOpenLoop(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t steeringCommand,
    uint32_t nowMs)
{
    int16_t limitedDrive;
    int16_t limitedSteering;
    int16_t leftOutput;
    int16_t rightOutput;

    if ((chassis == NULL) ||
        !chassis->initialized ||
        !chassis->enabled) {
        return;
    }

    limitedDrive =
        Chassis_ClampDrive(chassis, driveOutput);
    limitedSteering =
        Chassis_ClampSteering(chassis, steeringCommand);
    leftOutput = Chassis_ApplyOpenLoopScale(
        limitedDrive,
        chassis->config.leftOpenLoopScalePermille);
    rightOutput = Chassis_ApplyOpenLoopScale(
        limitedDrive,
        chassis->config.rightOpenLoopScalePermille);

    Motor_SetOutput(
        chassis->config.leftMotor, leftOutput, nowMs);
    Motor_SetOutput(
        chassis->config.rightMotor, rightOutput, nowMs);
    (void) Servo_SetNormalized(
        chassis->config.steeringServo, limitedSteering);

    chassis->driveOutput = limitedDrive;
    chassis->steeringCommand = limitedSteering;
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
}

void Chassis_CenterSteering(Chassis *chassis)
{
    if ((chassis == NULL) || !chassis->initialized) {
        return;
    }

    (void) Servo_Center(chassis->config.steeringServo);
    chassis->steeringCommand = 0;
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
