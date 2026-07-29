#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "motor.h"

#define IN1_PIN (1U << 0)
#define IN2_PIN (1U << 1)
#define STBY_PIN (1U << 2)
#define PWM_CHANNEL (0U)

typedef enum {
    OUTPUT_EVENT_GPIO_HIGH = 0,
    OUTPUT_EVENT_GPIO_LOW,
    OUTPUT_EVENT_PWM_DUTY,
} OutputEventType;

typedef struct {
    OutputEventType type;
    uint32_t value;
} OutputEvent;

static OutputEvent gOutputEvents[256];
static uint32_t gOutputEventCount;

static void RecordOutputEvent(
    OutputEventType type, uint32_t value)
{
    assert(gOutputEventCount <
        (sizeof(gOutputEvents) /
            sizeof(gOutputEvents[0])));

    gOutputEvents[gOutputEventCount].type = type;
    gOutputEvents[gOutputEventCount].value = value;
    gOutputEventCount++;
}

static void ClearOutputEvents(void)
{
    gOutputEventCount = 0U;
}

void DigitalOutput_High(GPIO_Regs *port, uint32_t pin)
{
    RecordOutputEvent(OUTPUT_EVENT_GPIO_HIGH, pin);
    port->level |= pin;
}

void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin)
{
    RecordOutputEvent(OUTPUT_EVENT_GPIO_LOW, pin);
    port->level &= ~pin;
}

void DigitalOutput_Write(
    GPIO_Regs *port, uint32_t pin, bool high)
{
    if (high) {
        DigitalOutput_High(port, pin);
    } else {
        DigitalOutput_Low(port, pin);
    }
}

void DigitalOutput_Toggle(GPIO_Regs *port, uint32_t pin)
{
    port->level ^= pin;
}

void PwmOutput_Start(GPTIMER_Regs *timer)
{
    (void) timer;
}

void PwmOutput_Stop(GPTIMER_Regs *timer)
{
    (void) timer;
}

void PwmOutput_SetDuty(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint16_t duty)
{
    assert(channel < 4U);
    RecordOutputEvent(OUTPUT_EVENT_PWM_DUTY, duty);
    timer->duty[channel] = duty;
}

static Motor_Config MakeConfig(
    GPIO_Regs *gpio,
    GPTIMER_Regs *timer,
    uint16_t maximumOutput,
    bool inverted,
    uint32_t reversalDelayMs)
{
    Motor_Config config;

    config.pwmTimer = timer;
    config.pwmChannel = PWM_CHANNEL;
    config.in1Port = gpio;
    config.in1Pin = IN1_PIN;
    config.in2Port = gpio;
    config.in2Pin = IN2_PIN;
    config.maximumOutput = maximumOutput;
    config.inverted = inverted;
    config.reversalDelayMs = reversalDelayMs;

    return config;
}

static void TestInitializationAndStandby(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{123U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Standby standby = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 700U, false, 5U);

    assert(Motor_Init(&motor, &config, 10U));
    assert(Motor_IsInitialized(&motor));
    assert(Motor_GetMode(&motor) == MOTOR_MODE_BRAKE);
    assert(timer.duty[PWM_CHANNEL] == 0U);
    assert((gpio.level & IN1_PIN) != 0U);
    assert((gpio.level & IN2_PIN) != 0U);

    assert(Motor_StandbyInit(
        &standby, &gpio, STBY_PIN));
    assert(!Motor_StandbyIsEnabled(&standby));
    assert((gpio.level & STBY_PIN) == 0U);

    Motor_StandbyEnable(&standby);
    assert(Motor_StandbyIsEnabled(&standby));
    assert((gpio.level & STBY_PIN) != 0U);

    Motor_StandbyDisable(&standby);
    assert(!Motor_StandbyIsEnabled(&standby));
    assert((gpio.level & STBY_PIN) == 0U);
}

static void TestLimitDirectionsAndStops(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 600U, false, 5U);
    assert(Motor_Init(&motor, &config, 0U));

    Motor_SetOutput(&motor, 900, 1U);
    assert(Motor_GetRequestedOutput(&motor) == 600);
    assert(Motor_GetAppliedOutput(&motor) == 600);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_FORWARD);
    assert(timer.duty[PWM_CHANNEL] == 600U);
    assert((gpio.level & IN1_PIN) != 0U);
    assert((gpio.level & IN2_PIN) == 0U);

    Motor_Brake(&motor, 2U);
    assert(Motor_GetAppliedOutput(&motor) == 0);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_BRAKE);
    assert(timer.duty[PWM_CHANNEL] == 0U);
    assert((gpio.level & IN1_PIN) != 0U);
    assert((gpio.level & IN2_PIN) != 0U);

    Motor_Coast(&motor, 3U);
    assert(Motor_GetAppliedOutput(&motor) == 0);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_COAST);
    assert(timer.duty[PWM_CHANNEL] == MOTOR_OUTPUT_MAX);
    assert((gpio.level & IN1_PIN) == 0U);
    assert((gpio.level & IN2_PIN) == 0U);
}

static void TestNonBlockingReversal(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 1000U, false, 5U);
    assert(Motor_Init(&motor, &config, 0U));

    Motor_Forward(&motor, 500U, 10U);
    Motor_Reverse(&motor, 700U, 20U);

    assert(Motor_IsReversalPending(&motor));
    assert(Motor_GetRequestedOutput(&motor) == -700);
    assert(Motor_GetAppliedOutput(&motor) == 0);
    assert(Motor_GetMode(&motor) ==
        MOTOR_MODE_REVERSAL_WAIT);
    assert(timer.duty[PWM_CHANNEL] == 0U);

    Motor_Reverse(&motor, 300U, 22U);
    assert(Motor_GetRequestedOutput(&motor) == -300);
    assert(Motor_IsReversalPending(&motor));

    Motor_Process(&motor, 24U);
    assert(Motor_IsReversalPending(&motor));
    assert(Motor_GetAppliedOutput(&motor) == 0);

    Motor_Process(&motor, 25U);
    assert(!Motor_IsReversalPending(&motor));
    assert(Motor_GetAppliedOutput(&motor) == -300);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_REVERSE);
    assert(timer.duty[PWM_CHANNEL] == 300U);
    assert((gpio.level & IN1_PIN) == 0U);
    assert((gpio.level & IN2_PIN) != 0U);
}

static void TestInversionAndCancellation(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 1000U, true, 10U);
    assert(Motor_Init(&motor, &config, 0U));

    Motor_Forward(&motor, 400U, 1U);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_FORWARD);
    assert(Motor_GetAppliedOutput(&motor) == 400);
    assert((gpio.level & IN1_PIN) == 0U);
    assert((gpio.level & IN2_PIN) != 0U);

    Motor_Reverse(&motor, 400U, 2U);
    assert(Motor_IsReversalPending(&motor));

    Motor_SetOutput(&motor, 0, 3U);
    assert(!Motor_IsReversalPending(&motor));
    assert(Motor_GetMode(&motor) == MOTOR_MODE_BRAKE);
    assert(Motor_GetRequestedOutput(&motor) == 0);
}

static void TestSafeRestartFromCoast(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 1000U, false, 5U);
    assert(Motor_Init(&motor, &config, 0U));
    Motor_Forward(&motor, 400U, 1U);
    Motor_Coast(&motor, 2U);
    assert(timer.duty[PWM_CHANNEL] == MOTOR_OUTPUT_MAX);

    ClearOutputEvents();
    Motor_Forward(&motor, 500U, 3U);

    /*
     * Coast 使用 PWM = 100%。重新驱动时必须先写 PWM = 0，
     * 再改变方向引脚，最后才写入目标占空比。
     */
    assert(gOutputEventCount == 4U);
    assert(gOutputEvents[0].type ==
        OUTPUT_EVENT_PWM_DUTY);
    assert(gOutputEvents[0].value == 0U);
    assert(gOutputEvents[1].type ==
        OUTPUT_EVENT_GPIO_HIGH);
    assert(gOutputEvents[1].value == IN1_PIN);
    assert(gOutputEvents[2].type ==
        OUTPUT_EVENT_GPIO_LOW);
    assert(gOutputEvents[2].value == IN2_PIN);
    assert(gOutputEvents[3].type ==
        OUTPUT_EVENT_PWM_DUTY);
    assert(gOutputEvents[3].value == 500U);

    ClearOutputEvents();
    Motor_Forward(&motor, 600U, 4U);

    /*
     * 已经同方向运行时只更新方向电平和占空比，不额外插入 PWM = 0。
     */
    assert(gOutputEventCount == 3U);
    assert(gOutputEvents[0].type ==
        OUTPUT_EVENT_GPIO_HIGH);
    assert(gOutputEvents[1].type ==
        OUTPUT_EVENT_GPIO_LOW);
    assert(gOutputEvents[2].type ==
        OUTPUT_EVENT_PWM_DUTY);
    assert(gOutputEvents[2].value == 600U);
}

static void TestMillisecondWraparound(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, 1000U, false, 5U);
    assert(Motor_Init(&motor, &config, 0U));

    Motor_Forward(&motor, 250U, UINT32_MAX - 10U);
    Motor_Reverse(&motor, 250U, UINT32_MAX - 2U);

    Motor_Process(&motor, 1U);
    assert(Motor_IsReversalPending(&motor));

    Motor_Process(&motor, 2U);
    assert(!Motor_IsReversalPending(&motor));
    assert(Motor_GetAppliedOutput(&motor) == -250);
}

static void TestInvalidAndLockedConfigurations(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}};
    Motor motor = {0};
    Motor_Config config;

    config = MakeConfig(
        &gpio, &timer, MOTOR_OUTPUT_MAX + 1U, false, 1U);
    assert(!Motor_Init(&motor, &config, 0U));

    config = MakeConfig(
        &gpio, &timer, 0U, false, 1U);
    assert(Motor_Init(&motor, &config, 0U));
    Motor_SetOutput(&motor, 1000, 1U);
    assert(Motor_GetRequestedOutput(&motor) == 0);
    assert(Motor_GetAppliedOutput(&motor) == 0);
    assert(Motor_GetMode(&motor) == MOTOR_MODE_BRAKE);
}

int main(void)
{
    TestInitializationAndStandby();
    TestLimitDirectionsAndStops();
    TestNonBlockingReversal();
    TestInversionAndCancellation();
    TestSafeRestartFromCoast();
    TestMillisecondWraparound();
    TestInvalidAndLockedConfigurations();

    puts("motor host tests passed");
    return 0;
}
