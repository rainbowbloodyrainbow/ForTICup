#include "output.h"

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
    uint32_t period;
    uint32_t highCounts;
    uint32_t compareValue;

    if (duty > PWM_OUTPUT_DUTY_MAX) {
        duty = PWM_OUTPUT_DUTY_MAX;
    }

    period = DL_Timer_getLoadValue(timer);

    highCounts =
        (uint32_t)(((uint64_t) period * duty) / PWM_OUTPUT_DUTY_MAX);
    compareValue = period - highCounts;

    DL_Timer_setCaptureCompareValue(timer, compareValue, channel);
}
