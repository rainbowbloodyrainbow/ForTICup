#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "application_policy.h"

static void TestForwardDriveLimit(void)
{
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        180, 90) == 90);
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        180, 300) == 180);
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        180, -300) == -180);
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        0, 50) == 0);
}

static void TestNegativeDriveUsesMagnitude(void)
{
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        -120, 200) == 120);
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        -120, -200) == -120);
    assert(ApplicationPolicy_LimitTurnForForwardDrive(
        INT16_MIN, INT16_MAX) == INT16_MAX);
}

int main(void)
{
    TestForwardDriveLimit();
    TestNegativeDriveUsesMagnitude();

    puts("application policy host tests passed");
    return 0;
}
