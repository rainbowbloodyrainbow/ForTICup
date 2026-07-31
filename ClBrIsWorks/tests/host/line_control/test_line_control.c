#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "line_control.h"

static LineControl_Config MakeConfig(bool inverted)
{
    LineControl_Config config = {
        .turnPid = {
            .kp = 1.0f,
            .ki = 0.0f,
            .kd = 0.0f,
            .integralMinimum = -1.0f,
            .integralMaximum = 1.0f,
            .outputMinimum = -1.0f,
            .outputMaximum = 1.0f,
            .derivativeFilterCoefficient = 0.0f
        },
        .positionFullScale = 2000.0f,
        .maximumTurnCommand = 800,
        .turnInverted = inverted,
        .maximumInvalidFrames = 3U
    };
    return config;
}

static void TestTrackingSignsAndLimit(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(false);
    LineSensor_Result line = {0};

    assert(LineControl_Init(&control, &config));
    line.valid = true;
    line.position = 0;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_TRACKING);
    assert(LineControl_GetTurnCommand(&control) == 0);

    line.position = -1000;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 400);

    line.position = 1000;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == -400);

    line.position = -4000;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 800);
}

static void TestInversion(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(true);
    LineSensor_Result line = {
        .position = -1000,
        .valid = true
    };

    assert(LineControl_Init(&control, &config));
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == -400);
}

static void TestInvalidFramesAndRecovery(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(false);
    LineSensor_Result line = {
        .position = -1000,
        .valid = true
    };

    assert(LineControl_Init(&control, &config));
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 400);

    line.valid = false;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_HOLDING);
    assert(LineControl_GetTurnCommand(&control) == 400);
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_HOLDING);
    assert(LineControl_GetTurnCommand(&control) == 400);
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_LINE_LOST);
    assert(LineControl_GetTurnCommand(&control) == 0);

    line.valid = true;
    line.position = 0;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_TRACKING);
    assert(LineControl_GetInvalidFrameCount(&control) == 0U);
}

static void TestBinaryPatternCommands(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(false);
    LineSensor_Result line = {
        .valid = true
    };

    config.maximumTurnCommand = 60;
    config.binaryPatternEnabled = true;
    config.binaryCorrectionCommand = 35;
    config.binarySharpCommand = 60;
    assert(LineControl_Init(&control, &config));

    line.strength[2] = LINE_SENSOR_STRENGTH_MAX;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_TRACKING);
    assert(LineControl_GetTurnCommand(&control) == 0);

    line.strength[2] = 0U;
    line.strength[1] = LINE_SENSOR_STRENGTH_MAX;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 35);

    line.strength[1] = 0U;
    line.strength[0] = LINE_SENSOR_STRENGTH_MAX;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 60);

    line.strength[0] = 0U;
    line.strength[4] = LINE_SENSOR_STRENGTH_MAX;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == -60);

    config.turnInverted = true;
    assert(LineControl_Init(&control, &config));
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetTurnCommand(&control) == 60);
}

int main(void)
{
    TestTrackingSignsAndLimit();
    TestInversion();
    TestInvalidFramesAndRecovery();
    TestBinaryPatternCommands();

    puts("line_control host tests passed");
    return 0;
}
