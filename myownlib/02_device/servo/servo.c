#include "servo.h"

#include <stddef.h>

static bool Servo_IsConfigValid(
    const Servo_Config *config)
{
    return (config != NULL) &&
        (config->pwmTimer != NULL) &&
        (config->timerClockHz != 0U) &&
        (config->minimumPulseUs <
            config->centerPulseUs) &&
        (config->centerPulseUs <
            config->maximumPulseUs);
}

static int16_t Servo_ClampCommand(int32_t command)
{
    if (command > SERVO_COMMAND_MAX) {
        command = SERVO_COMMAND_MAX;
    } else if (command < -SERVO_COMMAND_MAX) {
        command = -SERVO_COMMAND_MAX;
    }

    return (int16_t) command;
}

static uint16_t Servo_CommandToPulseUs(
    const Servo *servo, int16_t command)
{
    int32_t physicalCommand;
    int32_t span;
    int32_t pulse;

    physicalCommand = command;
    if (servo->config.inverted) {
        physicalCommand = -physicalCommand;
    }

    if (physicalCommand >= 0) {
        span =
            (int32_t) servo->config.maximumPulseUs -
            servo->config.centerPulseUs;
    } else {
        span =
            (int32_t) servo->config.centerPulseUs -
            servo->config.minimumPulseUs;
    }

    pulse =
        (int32_t) servo->config.centerPulseUs +
        (physicalCommand * span) / SERVO_COMMAND_MAX;
    return (uint16_t) pulse;
}

static bool Servo_ApplyPulseUs(
    Servo *servo, uint16_t pulseUs)
{
    return PwmOutput_SetPulseUs(
        servo->config.pwmTimer,
        servo->config.pwmChannel,
        servo->config.timerClockHz,
        pulseUs);
}

bool Servo_Init(
    Servo *servo, const Servo_Config *config)
{
    if ((servo == NULL) ||
        !Servo_IsConfigValid(config)) {
        return false;
    }

    servo->config = *config;
    servo->command = 0;
    servo->pulseUs = config->centerPulseUs;
    servo->enabled = false;
    servo->initialized = true;

    if (!Servo_ApplyPulseUs(
            servo, servo->pulseUs)) {
        servo->initialized = false;
        return false;
    }

    return true;
}

bool Servo_Enable(Servo *servo)
{
    if ((servo == NULL) || !servo->initialized) {
        return false;
    }

    if (!Servo_ApplyPulseUs(servo, servo->pulseUs)) {
        return false;
    }

    PwmOutput_Start(servo->config.pwmTimer);
    servo->enabled = true;
    return true;
}

void Servo_Disable(Servo *servo)
{
    if ((servo == NULL) || !servo->initialized) {
        return;
    }

    PwmOutput_Stop(servo->config.pwmTimer);
    servo->enabled = false;
}

bool Servo_SetNormalized(
    Servo *servo, int16_t command)
{
    int16_t limitedCommand;
    uint16_t pulseUs;

    if ((servo == NULL) || !servo->initialized) {
        return false;
    }

    limitedCommand = Servo_ClampCommand(command);
    pulseUs =
        Servo_CommandToPulseUs(servo, limitedCommand);

    if (!Servo_ApplyPulseUs(servo, pulseUs)) {
        return false;
    }

    servo->command = limitedCommand;
    servo->pulseUs = pulseUs;
    return true;
}

bool Servo_SetPulseUs(
    Servo *servo, uint16_t pulseUs)
{
    if ((servo == NULL) ||
        !servo->initialized ||
        (pulseUs < servo->config.minimumPulseUs) ||
        (pulseUs > servo->config.maximumPulseUs) ||
        !Servo_ApplyPulseUs(servo, pulseUs)) {
        return false;
    }

    servo->pulseUs = pulseUs;
    return true;
}

bool Servo_Center(Servo *servo)
{
    return Servo_SetNormalized(servo, 0);
}

int16_t Servo_GetCommand(const Servo *servo)
{
    if ((servo == NULL) || !servo->initialized) {
        return 0;
    }
    return servo->command;
}

uint16_t Servo_GetPulseUs(const Servo *servo)
{
    if ((servo == NULL) || !servo->initialized) {
        return 0U;
    }
    return servo->pulseUs;
}

bool Servo_IsEnabled(const Servo *servo)
{
    return (servo != NULL) &&
        servo->initialized &&
        servo->enabled;
}
