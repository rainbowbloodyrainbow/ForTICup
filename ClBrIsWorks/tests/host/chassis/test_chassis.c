#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chassis.h"

#define LEFT_IN1_PIN (1U << 0)
#define LEFT_IN2_PIN (1U << 1)
#define RIGHT_IN1_PIN (1U << 2)
#define RIGHT_IN2_PIN (1U << 3)
#define STANDBY_PIN (1U << 4)

void DigitalOutput_High(GPIO_Regs *port, uint32_t pin)
{
    port->level |= pin;
}

void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin)
{
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
    timer->startCount++;
}

void PwmOutput_Stop(GPTIMER_Regs *timer)
{
    timer->stopCount++;
}

void PwmOutput_SetDuty(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint16_t duty)
{
    assert(channel < 4U);
    timer->duty[channel] = duty;
}

static Motor_Config MakeMotorConfig(
    GPIO_Regs *gpio,
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint32_t in1Pin,
    uint32_t in2Pin)
{
    Motor_Config config = {
        .pwmTimer = timer,
        .pwmChannel = channel,
        .in1Port = gpio,
        .in1Pin = in1Pin,
        .in2Port = gpio,
        .in2Pin = in2Pin,
        .maximumOutput = 350U,
        .inverted = false,
        .reversalDelayMs = 5U
    };
    return config;
}

static Chassis_Config MakeChassisConfig(
    Motor *left,
    Motor *right,
    Motor_Standby *standby)
{
    Chassis_Config config = {
        .leftMotor = left,
        .rightMotor = right,
        .standby = standby,
        .maximumDriveOutput = 350U,
        .maximumTurnOutput = 100U,
        .leftOpenLoopScalePermille = 900U,
        .rightOpenLoopScalePermille = 1000U
    };
    return config;
}

static void TestDifferentialMixAndSafetyClamp(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}, 0U, 0U};
    Motor left = {0};
    Motor right = {0};
    Motor_Standby standby = {0};
    Chassis chassis = {0};
    Motor_Config leftConfig;
    Motor_Config rightConfig;
    Chassis_Config chassisConfig;

    leftConfig = MakeMotorConfig(
        &gpio, &timer, 0U,
        LEFT_IN1_PIN, LEFT_IN2_PIN);
    rightConfig = MakeMotorConfig(
        &gpio, &timer, 1U,
        RIGHT_IN1_PIN, RIGHT_IN2_PIN);
    assert(Motor_StandbyInit(
        &standby, &gpio, STANDBY_PIN));
    assert(Motor_Init(&left, &leftConfig, 0U));
    assert(Motor_Init(&right, &rightConfig, 0U));

    chassisConfig =
        MakeChassisConfig(&left, &right, &standby);
    assert(Chassis_Init(&chassis, &chassisConfig));
    assert(Chassis_Enable(&chassis, 0U));
    assert(timer.startCount == 1U);
    assert(Motor_StandbyIsEnabled(&standby));

    Chassis_SetDriveTurn(&chassis, 100, 30, 1U);
    assert(Motor_GetAppliedOutput(&left) == 63);
    assert(Motor_GetAppliedOutput(&right) == 130);
    assert(chassis.turnOutput == 30);

    Chassis_SetDriveTurn(&chassis, 20, 80, 2U);
    assert(Motor_GetRequestedOutput(&left) == -54);
    assert(Motor_GetAppliedOutput(&right) == 100);

    Chassis_SetDriveTurn(&chassis, 100, -30, 3U);
    assert(Motor_GetAppliedOutput(&left) == 117);
    assert(Motor_GetAppliedOutput(&right) == 70);

    Chassis_SetDriveTurn(&chassis, 300, 100, 4U);
    assert(Motor_GetRequestedOutput(&left) == 157);
    assert(Motor_GetRequestedOutput(&right) == 350);
    assert(Chassis_GetLeftOutput(&chassis) == 157);
    assert(Chassis_GetRightOutput(&chassis) == 350);

    Chassis_SetDriveTurn(&chassis, 100, 500, 5U);
    assert(Motor_GetAppliedOutput(&left) == 0);
    assert(Motor_GetAppliedOutput(&right) == 200);
    assert(chassis.turnOutput == 100);

    Chassis_SetWheelOutputs(&chassis, -50, 400, 6U);
    assert(Motor_GetRequestedOutput(&left) == -38);
    assert(Motor_GetRequestedOutput(&right) == 350);
    assert(Chassis_GetDriveOutput(&chassis) == 153);
    assert(Chassis_GetTurnOutput(&chassis) == 196);

    Chassis_Brake(&chassis, 7U);
    assert(Motor_GetMode(&left) == MOTOR_MODE_BRAKE);
    assert(Motor_GetMode(&right) == MOTOR_MODE_BRAKE);

    Chassis_SetDriveTurn(&chassis, -100, 30, 8U);
    assert(Motor_GetRequestedOutput(&left) == -117);
    assert(Motor_GetRequestedOutput(&right) == -70);
    Chassis_Process(&chassis, 13U);
    assert(Motor_GetAppliedOutput(&left) == -117);
    assert(Motor_GetAppliedOutput(&right) == -70);

    Chassis_Disable(&chassis, 14U);
    assert(!Motor_StandbyIsEnabled(&standby));
    assert(timer.stopCount == 1U);
}

static void TestOptionalStandby(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}, 0U, 0U};
    Motor left = {0};
    Motor right = {0};
    Chassis chassis = {0};
    Motor_Config leftConfig;
    Motor_Config rightConfig;
    Chassis_Config chassisConfig;

    leftConfig = MakeMotorConfig(
        &gpio, &timer, 0U,
        LEFT_IN1_PIN, LEFT_IN2_PIN);
    rightConfig = MakeMotorConfig(
        &gpio, &timer, 1U,
        RIGHT_IN1_PIN, RIGHT_IN2_PIN);
    assert(Motor_Init(&left, &leftConfig, 0U));
    assert(Motor_Init(&right, &rightConfig, 0U));

    chassisConfig =
        MakeChassisConfig(&left, &right, NULL);
    assert(Chassis_Init(&chassis, &chassisConfig));
    assert(Chassis_Enable(&chassis, 0U));
    assert(chassis.enabled);
    assert(timer.startCount == 1U);

    Chassis_SetDriveTurn(&chassis, 100, 30, 1U);
    assert(Motor_GetAppliedOutput(&left) == 63);
    assert(Motor_GetAppliedOutput(&right) == 130);

    Chassis_Brake(&chassis, 2U);
    assert(Motor_GetMode(&left) == MOTOR_MODE_BRAKE);
    assert(Motor_GetMode(&right) == MOTOR_MODE_BRAKE);

    Chassis_Disable(&chassis, 3U);
    assert(!chassis.enabled);
    assert(timer.stopCount == 1U);
    assert(Motor_GetMode(&left) == MOTOR_MODE_BRAKE);
    assert(Motor_GetMode(&right) == MOTOR_MODE_BRAKE);
}

static void TestInvalidTurnLimit(void)
{
    GPIO_Regs gpio = {0U};
    GPTIMER_Regs timer = {{0U, 0U, 0U, 0U}, 0U, 0U};
    Motor left = {0};
    Motor right = {0};
    Motor_Standby standby = {0};
    Chassis chassis = {0};
    Motor_Config leftConfig;
    Motor_Config rightConfig;
    Chassis_Config chassisConfig;

    leftConfig = MakeMotorConfig(
        &gpio, &timer, 0U,
        LEFT_IN1_PIN, LEFT_IN2_PIN);
    rightConfig = MakeMotorConfig(
        &gpio, &timer, 1U,
        RIGHT_IN1_PIN, RIGHT_IN2_PIN);
    assert(Motor_StandbyInit(
        &standby, &gpio, STANDBY_PIN));
    assert(Motor_Init(&left, &leftConfig, 0U));
    assert(Motor_Init(&right, &rightConfig, 0U));

    chassisConfig =
        MakeChassisConfig(&left, &right, &standby);
    chassisConfig.maximumTurnOutput = 351U;
    assert(!Chassis_Init(&chassis, &chassisConfig));
}

int main(void)
{
    TestDifferentialMixAndSafetyClamp();
    TestOptionalStandby();
    TestInvalidTurnLimit();

    puts("chassis host tests passed");
    return 0;
}
