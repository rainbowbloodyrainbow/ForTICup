#include "application_policy.h"

#include <stdint.h>

int16_t ApplicationPolicy_LimitTurnForForwardDrive(
    int16_t driveOutput, int16_t turnOutput)
{
    int32_t maximumTurn;
    int32_t limitedTurn;

    maximumTurn = driveOutput;
    if (maximumTurn < 0) {
        maximumTurn = -maximumTurn;
    }

    limitedTurn = turnOutput;
    if (limitedTurn > maximumTurn) {
        limitedTurn = maximumTurn;
    } else if (limitedTurn < -maximumTurn) {
        limitedTurn = -maximumTurn;
    }

    return (int16_t) limitedTurn;
}
