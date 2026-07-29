#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <stdint.h>

void SystemTime_On1msTick(void);

uint32_t SystemTime_GetMs(void);
uint32_t SystemTime_GetControlSequence(void);
uint32_t SystemTime_ElapsedMs(
    uint32_t now, uint32_t previous);

#endif
