#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t level;
} GPIO_Regs;

typedef struct {
    uint16_t duty[4];
} GPTIMER_Regs;

typedef uint32_t DL_TIMER_CC_INDEX;

void DigitalOutput_High(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Write(
    GPIO_Regs *port, uint32_t pin, bool high);
void DigitalOutput_Toggle(GPIO_Regs *port, uint32_t pin);

void PwmOutput_Start(GPTIMER_Regs *timer);
void PwmOutput_Stop(GPTIMER_Regs *timer);
void PwmOutput_SetDuty(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint16_t duty);

#endif
