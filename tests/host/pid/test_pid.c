#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "pid.h"

#define EPSILON (0.0001f)

static bool NearlyEqual(float left, float right)
{
    return fabsf(left - right) < EPSILON;
}

static PID_Config MakeConfig(void)
{
    PID_Config config = {
        .kp = 2.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integralMinimum = -1.0f,
        .integralMaximum = 1.0f,
        .outputMinimum = -5.0f,
        .outputMaximum = 5.0f,
        .derivativeFilterCoefficient = 1.0f
    };
    return config;
}

static void TestProportionalAndOutputLimits(void)
{
    PID pid = {0};
    PID_Config config = MakeConfig();

    assert(PID_Init(&pid, &config));
    assert(NearlyEqual(
        PID_UpdateError(&pid, 1.5f, 0.1f), 3.0f));
    assert(NearlyEqual(
        PID_UpdateError(&pid, 10.0f, 0.1f), 5.0f));
    assert(NearlyEqual(
        PID_UpdateError(&pid, -10.0f, 0.1f), -5.0f));
}

static void TestIntegralLimit(void)
{
    PID pid = {0};
    PID_Config config = MakeConfig();
    uint32_t index;

    config.kp = 0.0f;
    config.ki = 2.0f;
    assert(PID_Init(&pid, &config));

    for (index = 0U; index < 10U; index++) {
        (void) PID_UpdateError(&pid, 1.0f, 1.0f);
    }
    assert(NearlyEqual(pid.integral, 1.0f));
    assert(NearlyEqual(PID_GetLastOutput(&pid), 2.0f));
}

static void TestDerivativeAndZeroDt(void)
{
    PID pid = {0};
    PID_Config config = MakeConfig();

    config.kp = 0.0f;
    config.kd = 1.0f;
    assert(PID_Init(&pid, &config));

    assert(NearlyEqual(
        PID_UpdateError(&pid, 4.0f, 0.1f), 0.0f));
    assert(NearlyEqual(
        PID_UpdateError(&pid, 5.0f, 0.0f), 0.0f));
    assert(NearlyEqual(
        PID_UpdateError(&pid, 5.0f, 0.1f), 10.0f - 5.0f));
}

static void TestReset(void)
{
    PID pid = {0};
    PID_Config config = MakeConfig();

    assert(PID_Init(&pid, &config));
    (void) PID_UpdateError(&pid, 1.0f, 0.5f);
    assert(pid.hasPreviousError);
    assert(pid.integral != 0.0f);

    PID_Reset(&pid);
    assert(!pid.hasPreviousError);
    assert(NearlyEqual(pid.integral, 0.0f));
    assert(NearlyEqual(pid.filteredDerivative, 0.0f));
    assert(NearlyEqual(pid.lastOutput, 0.0f));
}

int main(void)
{
    TestProportionalAndOutputLimits();
    TestIntegralLimit();
    TestDerivativeAndZeroDt();
    TestReset();

    puts("pid host tests passed");
    return 0;
}
