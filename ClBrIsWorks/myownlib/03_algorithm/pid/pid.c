#include "pid.h"

#include <math.h>
#include <stddef.h>

static float PID_Clamp(
    float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

static bool PID_IsConfigValid(
    const PID_Config *config)
{
    if (config == NULL) {
        return false;
    }

    return isfinite(config->kp) &&
        isfinite(config->ki) &&
        isfinite(config->kd) &&
        isfinite(config->integralMinimum) &&
        isfinite(config->integralMaximum) &&
        isfinite(config->outputMinimum) &&
        isfinite(config->outputMaximum) &&
        isfinite(config->derivativeFilterCoefficient) &&
        (config->integralMinimum <=
            config->integralMaximum) &&
        (config->outputMinimum <=
            config->outputMaximum) &&
        (config->derivativeFilterCoefficient >= 0.0f) &&
        (config->derivativeFilterCoefficient <= 1.0f);
}

bool PID_Init(PID *pid, const PID_Config *config)
{
    if ((pid == NULL) || !PID_IsConfigValid(config)) {
        return false;
    }

    pid->config = *config;
    pid->initialized = true;
    PID_Reset(pid);
    return true;
}

void PID_Reset(PID *pid)
{
    if ((pid == NULL) || !pid->initialized) {
        return;
    }

    pid->integral = 0.0f;
    pid->previousError = 0.0f;
    pid->filteredDerivative = 0.0f;
    pid->lastOutput = 0.0f;
    pid->hasPreviousError = false;
}

float PID_UpdateError(
    PID *pid, float error, float dtSeconds)
{
    float derivative = 0.0f;
    float output;
    float coefficient;

    if ((pid == NULL) ||
        !pid->initialized ||
        !isfinite(error) ||
        !isfinite(dtSeconds)) {
        return 0.0f;
    }

    if (dtSeconds <= 0.0f) {
        return pid->lastOutput;
    }

    pid->integral = PID_Clamp(
        pid->integral + error * dtSeconds,
        pid->config.integralMinimum,
        pid->config.integralMaximum);

    if (pid->hasPreviousError) {
        coefficient =
            pid->config.derivativeFilterCoefficient;
        if (coefficient > 0.0f) {
            derivative =
                (error - pid->previousError) / dtSeconds;
            pid->filteredDerivative +=
                coefficient *
                (derivative - pid->filteredDerivative);
        }
    } else {
        pid->hasPreviousError = true;
    }

    pid->previousError = error;
    output =
        pid->config.kp * error +
        pid->config.ki * pid->integral +
        pid->config.kd * pid->filteredDerivative;
    pid->lastOutput = PID_Clamp(
        output,
        pid->config.outputMinimum,
        pid->config.outputMaximum);

    return pid->lastOutput;
}

float PID_Update(
    PID *pid,
    float target,
    float measurement,
    float dtSeconds)
{
    return PID_UpdateError(
        pid, target - measurement, dtSeconds);
}

float PID_GetLastOutput(const PID *pid)
{
    if ((pid == NULL) || !pid->initialized) {
        return 0.0f;
    }
    return pid->lastOutput;
}
