#include "system_time.h"

#define SYSTEM_TIME_CONTROL_DIVIDER (10U)

static volatile uint32_t gMilliseconds;
static volatile uint32_t gControlSequence;
static uint8_t gControlDivider;

void SystemTime_On1msTick(void)
{
    gMilliseconds++;

    gControlDivider++;
    if (gControlDivider >= SYSTEM_TIME_CONTROL_DIVIDER) {
        gControlDivider = 0U;
        gControlSequence++;
    }
}

uint32_t SystemTime_GetMs(void)
{
    return gMilliseconds;
}

uint32_t SystemTime_GetControlSequence(void)
{
    return gControlSequence;
}

uint32_t SystemTime_ElapsedMs(
    uint32_t now, uint32_t previous)
{
    return now - previous;
}
