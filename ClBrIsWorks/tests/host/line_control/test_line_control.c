#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "line_control.h"

static LineControl_Config MakeConfig(bool inverted)
{
    LineControl_Config config = {
        .steeringPid = {
            .kp = 1.0f,
            .ki = 0.0f,
            .kd = 0.0f,
            .integralMinimum = -1.0f,
            .integralMaximum = 1.0f,
            .outputMinimum = -1.0f,
            .outputMaximum = 1.0f,
            .derivativeFilterCoefficient = 0.0f
        },
        .positionFullScale = 3500.0f,
        .maximumSteeringCommand = 800,
        .steeringInverted = inverted,
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
    assert(LineControl_GetSteeringCommand(&control) == 0);

    line.position = -1750;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetSteeringCommand(&control) == 400);

    line.position = 1750;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetSteeringCommand(&control) == -400);

    line.position = -7000;
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetSteeringCommand(&control) == 800);
}

static void TestInversion(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(true);
    LineSensor_Result line = {
        .position = -1750,
        .valid = true
    };

    assert(LineControl_Init(&control, &config));
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetSteeringCommand(&control) == -400);
}

static void TestInvalidFramesAndRecovery(void)
{
    LineControl control = {0};
    LineControl_Config config = MakeConfig(false);
    LineSensor_Result line = {
        .position = -1750,
        .valid = true
    };

    assert(LineControl_Init(&control, &config));
    (void) LineControl_Update(&control, &line, 0.01f);
    assert(LineControl_GetSteeringCommand(&control) == 400);

    line.valid = false;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_HOLDING);
    assert(LineControl_GetSteeringCommand(&control) == 400);
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_HOLDING);
    assert(LineControl_GetSteeringCommand(&control) == 400);
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_LINE_LOST);
    assert(LineControl_GetSteeringCommand(&control) == 0);

    line.valid = true;
    line.position = 0;
    assert(LineControl_Update(
        &control, &line, 0.01f) ==
        LINE_CONTROL_TRACKING);
    assert(LineControl_GetInvalidFrameCount(&control) == 0U);
}

int main(void)
{
    TestTrackingSignsAndLimit();
    TestInversion();
    TestInvalidFramesAndRecovery();

    puts("line_control host tests passed");
    return 0;
}
