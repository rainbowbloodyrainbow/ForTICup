#include "line_control.h"

#include <math.h>
#include <stddef.h>

static bool LineControl_IsConfigValid(
    const LineControl_Config *config)
{
    bool basicValid;

    basicValid = (config != NULL) &&
        isfinite(config->positionFullScale) &&
        (config->positionFullScale > 0.0f) &&
        (config->maximumTurnCommand > 0) &&
        (config->maximumInvalidFrames > 0U);
    if (!basicValid) {
        return false;
    }

    return !config->binaryPatternEnabled ||
        ((config->binaryCorrectionCommand > 0) &&
            (config->binarySharpCommand >=
                config->binaryCorrectionCommand) &&
            (config->binarySharpCommand <=
                config->maximumTurnCommand));
}

static int16_t LineControl_ClampTurn(
    const LineControl *control, int32_t command)
{
    int32_t maximum;

    maximum = control->config.maximumTurnCommand;
    if (command > maximum) {
        command = maximum;
    } else if (command < -maximum) {
        command = -maximum;
    }

    return (int16_t) command;
}

static int16_t LineControl_OutputToTurnCommand(
    const LineControl *control, float pidOutput)
{
    float scaled;
    int32_t command;

    scaled =
        pidOutput *
        control->config.maximumTurnCommand;
    if (scaled >= 0.0f) {
        command = (int32_t) (scaled + 0.5f);
    } else {
        command = (int32_t) (scaled - 0.5f);
    }

    if (control->config.turnInverted) {
        command = -command;
    }

    return LineControl_ClampTurn(control, command);
}

static int16_t LineControl_BinaryPatternToCommand(
    const LineControl *control,
    const LineSensor_Result *line)
{
    int32_t error;
    int32_t command;

    error =
        2 * (line->strength[0] != 0U) +
        (line->strength[1] != 0U) -
        (line->strength[3] != 0U) -
        2 * (line->strength[4] != 0U);

    if (error >= 2) {
        command = control->config.binarySharpCommand;
    } else if (error == 1) {
        command = control->config.binaryCorrectionCommand;
    } else if (error == -1) {
        command = -control->config.binaryCorrectionCommand;
    } else if (error <= -2) {
        command = -control->config.binarySharpCommand;
    } else {
        command = 0;
    }

    if (control->config.turnInverted) {
        command = -command;
    }
    return LineControl_ClampTurn(control, command);
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
            &control->turnPid,
            &config->turnPid)) {
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

    PID_Reset(&control->turnPid);
    control->turnCommand = 0;
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
            control->turnCommand = 0;
            control->status = LINE_CONTROL_LINE_LOST;
        } else {
            control->status = LINE_CONTROL_HOLDING;
        }
        return control->status;
    }

    control->consecutiveInvalidFrames = 0U;
    if (control->config.binaryPatternEnabled) {
        control->turnCommand =
            LineControl_BinaryPatternToCommand(
                control, line);
        control->status = LINE_CONTROL_TRACKING;
        return control->status;
    }

    normalizedPosition =
        (float) line->position /
        control->config.positionFullScale;
    error = -normalizedPosition;
    pidOutput = PID_UpdateError(
        &control->turnPid, error, dtSeconds);
    control->turnCommand =
        LineControl_OutputToTurnCommand(
            control, pidOutput);
    control->status = LINE_CONTROL_TRACKING;

    return control->status;
}

int16_t LineControl_GetTurnCommand(
    const LineControl *control)
{
    if ((control == NULL) || !control->initialized) {
        return 0;
    }
    return control->turnCommand;
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
