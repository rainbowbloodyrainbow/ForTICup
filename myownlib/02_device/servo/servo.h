#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>
#include <stdint.h>

#include "output.h"

#define SERVO_COMMAND_MAX (1000)

typedef struct {
    GPTIMER_Regs *pwmTimer;
    DL_TIMER_CC_INDEX pwmChannel;
    uint32_t timerClockHz;
    uint16_t minimumPulseUs;
    uint16_t centerPulseUs;
    uint16_t maximumPulseUs;
    bool inverted;
} Servo_Config;

typedef struct {
    Servo_Config config;
    int16_t command;
    uint16_t pulseUs;
    bool enabled;
    bool initialized;
} Servo;

bool Servo_Init(
    Servo *servo, const Servo_Config *config);
bool Servo_Enable(Servo *servo);
void Servo_Disable(Servo *servo);
bool Servo_SetNormalized(
    Servo *servo, int16_t command);
bool Servo_SetPulseUs(
    Servo *servo, uint16_t pulseUs);
bool Servo_Center(Servo *servo);
int16_t Servo_GetCommand(const Servo *servo);
uint16_t Servo_GetPulseUs(const Servo *servo);
bool Servo_IsEnabled(const Servo *servo);

#endif
