#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "system_time.h"

int main(void)
{
    uint32_t index;

    assert(SystemTime_GetMs() == 0U);
    assert(SystemTime_GetControlSequence() == 0U);

    for (index = 0U; index < 9U; index++) {
        SystemTime_On1msTick();
    }
    assert(SystemTime_GetMs() == 9U);
    assert(SystemTime_GetControlSequence() == 0U);

    SystemTime_On1msTick();
    assert(SystemTime_GetControlSequence() == 1U);

    for (index = 0U; index < 10U; index++) {
        SystemTime_On1msTick();
    }
    assert(SystemTime_GetControlSequence() == 2U);

    assert(SystemTime_ElapsedMs(3U, UINT32_MAX - 1U) == 5U);

    puts("system_time host tests passed");
    return 0;
}
