#include "line_control.h"

#include <math.h>
#include <stddef.h>

static bool LineControl_IsConfigValid(
    const LineControl_Config *config)
{
    return (config != NULL) &&
        isfinite(config->positionFullScale) &&
        (config->positionFullScale > 0.0f) &&
        (config->maximumSteeringCommand > 0) &&
        (config->maximumInvalidFrames > 0U);
}

static int16_t LineControl_ClampSteering(
    const LineControl *control, int32_t command)
{
    int32_t maximum;

    maximum = control->config.maximumSteeringCommand;
    if (command > maximum) {
        command = maximum;
    } else if (command < -maximum) {
        command = -maximum;
    }

    return (int16_t) command;
}

static int16_t LineControl_OutputToCommand(
    const LineControl *control, float pidOutput)
{
    float scaled;
    int32_t command;

    scaled =
        pidOutput *
        control->config.maximumSteeringCommand;
    if (scaled >= 0.0f) {
        command = (int32_t) (scaled + 0.5f);
    } else {
        command = (int32_t) (scaled - 0.5f);
    }

    if (control->config.steeringInverted) {
        command = -command;
    }

    return LineControl_ClampSteering(control, command);
}

bool LineControl_Init(
    LineControl *control,
    const LineControl_Config *config)
{
    if ((control == NULL) ||
        !LineControl_IsConfigValid(config)) {
        return false;
    }

    control->config = *config;
    if (!PID_Init(
            &control->steeringPid,
            &config->steeringPid)) {
        return false;
    }

    control->initialized = true;
    LineControl_Reset(control);
    return true;
}

void LineControl_Reset(LineControl *control)
{
    if ((control == NULL) || !control->initialized) {
        return;
    }

    PID_Reset(&control->steeringPid);
    control->steeringCommand = 0;
    control->consecutiveInvalidFrames = 0U;
    control->status = LINE_CONTROL_TRACKING;
}

LineControl_Status LineControl_Update(
    LineControl *control,
    const LineSensor_Result *line,
    float dtSeconds)
{
    float normalizedPosition;
    float error;
    float pidOutput;

    if ((control == NULL) || !control->initialized) {
        return LINE_CONTROL_INVALID_ARGUMENT;
    }
    if ((line == NULL) ||
        !isfinite(dtSeconds) ||
        (dtSeconds <= 0.0f)) {
        control->status =
            LINE_CONTROL_INVALID_ARGUMENT;
        return control->status;
    }

    if (!line->valid) {
        if (control->consecutiveInvalidFrames <
            UINT8_MAX) {
            control->consecutiveInvalidFrames++;
        }

        if (control->consecutiveInvalidFrames >=
            control->config.maximumInvalidFrames) {
            control->steeringCommand = 0;
            control->status = LINE_CONTROL_LINE_LOST;
        } else {
            control->status = LINE_CONTROL_HOLDING;
        }
        return control->status;
    }

    control->consecutiveInvalidFrames = 0U;
    normalizedPosition =
        (float) line->position /
        control->config.positionFullScale;
    error = -normalizedPosition;
    pidOutput = PID_UpdateError(
        &control->steeringPid, error, dtSeconds);
    control->steeringCommand =
        LineControl_OutputToCommand(control, pidOutput);
    control->status = LINE_CONTROL_TRACKING;

    return control->status;
}

int16_t LineControl_GetSteeringCommand(
    const LineControl *control)
{
    if ((control == NULL) || !control->initialized) {
        return 0;
    }
    return control->steeringCommand;
}

LineControl_Status LineControl_GetStatus(
    const LineControl *control)
{
    if ((control == NULL) || !control->initialized) {
        return LINE_CONTROL_INVALID_ARGUMENT;
    }
    return control->status;
}

uint8_t LineControl_GetInvalidFrameCount(
    const LineControl *control)
{
    if ((control == NULL) || !control->initialized) {
        return 0U;
    }
    return control->consecutiveInvalidFrames;
}
