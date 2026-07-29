#include "output.h"

#include <limits.h>
#include <stddef.h>

static bool PwmOutput_GetPeriodTicks(
    GPTIMER_Regs *timer,
    uint32_t *periodTicks)
{
    uint32_t loadValue;

    if ((timer == NULL) || (periodTicks == NULL)) {
        return false;
    }

    loadValue = DL_Timer_getLoadValue(timer);
    if (loadValue == UINT32_MAX) {
        return false;
    }

    /*
     * DriverLib 的边沿对齐 PWM 用 LOAD = period - 1。
     * SysConfig 的零占空比比较值使用 period，而不是 LOAD。
     */
    *periodTicks = loadValue + 1U;
    return true;
}

static bool PwmOutput_HighTicksToCompare(
    uint32_t periodTicks,
    uint32_t highTicks,
    uint32_t *compareValue)
{
    if ((compareValue == NULL) ||
        (highTicks > periodTicks)) {
        return false;
    }

    *compareValue = periodTicks - highTicks;
    return true;
}

void DigitalOutput_High(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_setPins(port, pin);
}

void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_clearPins(port, pin);
}

void DigitalOutput_Write(GPIO_Regs *port, uint32_t pin, bool high)
{
    if (high) {
        DigitalOutput_High(port, pin);
    } else {
        DigitalOutput_Low(port, pin);
    }
}

void DigitalOutput_Toggle(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_togglePins(port, pin);
}

void PwmOutput_Start(GPTIMER_Regs *timer)
{
    DL_Timer_startCounter(timer);
}

void PwmOutput_Stop(GPTIMER_Regs *timer)
{
    DL_Timer_stopCounter(timer);
}

void PwmOutput_SetDuty(
    GPTIMER_Regs *timer, DL_TIMER_CC_INDEX channel, uint16_t duty)
{
    uint32_t periodTicks;
    uint32_t highCounts;

    if (timer == NULL) {
        return;
    }

    if (duty > PWM_OUTPUT_DUTY_MAX) {
        duty = PWM_OUTPUT_DUTY_MAX;
    }

    if (!PwmOutput_GetPeriodTicks(timer, &periodTicks)) {
        return;
    }

    highCounts = (uint32_t)
        (((uint64_t) periodTicks * duty) /
            PWM_OUTPUT_DUTY_MAX);
    (void) PwmOutput_SetHighTicks(
        timer, channel, highCounts);
}

bool PwmOutput_SetHighTicks(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint32_t highTicks)
{
    uint32_t periodTicks;
    uint32_t compareValue;

    if (!PwmOutput_GetPeriodTicks(timer, &periodTicks) ||
        !PwmOutput_HighTicksToCompare(
            periodTicks, highTicks, &compareValue)) {
        return false;
    }

    DL_Timer_setCaptureCompareValue(
        timer, compareValue, channel);
    return true;
}

bool PwmOutput_SetPulseUs(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint32_t timerClockHz,
    uint32_t pulseUs)
{
    uint64_t ticks64;

    if ((timer == NULL) || (timerClockHz == 0U)) {
        return false;
    }

    ticks64 =
        ((uint64_t) timerClockHz * pulseUs) /
        1000000ULL;

    if (ticks64 > UINT32_MAX) {
        return false;
    }

    return PwmOutput_SetHighTicks(
        timer, channel, (uint32_t) ticks64);
}
