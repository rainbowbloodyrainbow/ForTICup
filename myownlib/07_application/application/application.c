#include "application.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adc.h"
#include "application_config.h"
#include "chassis.h"
#include "hc05.h"
#include "line_control.h"
#include "line_sensor.h"
#include "motor.h"
#include "servo.h"
#include "system_time.h"
#include "ti_msp_dl_config.h"

#define APPLICATION_CONTROL_DT_SECONDS (0.01f)

static Application_State gState;
static uint32_t gProcessedControlSequence;
static uint32_t gMissedControlCycles;
static uint32_t gAdcErrorCount;
static uint32_t gLastTelemetryMs;
static bool gTelemetryRequested;
static int16_t gDriveOutput;

static Motor gLeftMotor;
static Motor gRightMotor;
static Motor_Standby gMotorStandby;
static Servo gSteeringServo;
static LineSensor gLineSensor;
static LineControl gLineControl;
static Chassis gChassis;

static void Application_EnterState(
    Application_State newState)
{
    gState = newState;
}

static void Application_ApplySafeState(uint32_t nowMs)
{
    Chassis_Brake(&gChassis, nowMs);
    Chassis_CenterSteering(&gChassis);
}

static void Application_HandleMissedControlCycles(
    uint32_t missedCount)
{
    if ((UINT32_MAX - gMissedControlCycles) <
        missedCount) {
        gMissedControlCycles = UINT32_MAX;
    } else {
        gMissedControlCycles += missedCount;
    }
}

static void Application_RunLineFollowing(uint32_t nowMs)
{
    LineControl_Status status;
    int16_t steering;

    status = LineControl_Update(
        &gLineControl,
        LineSensor_GetResult(&gLineSensor),
        APPLICATION_CONTROL_DT_SECONDS);

    if (status == LINE_CONTROL_LINE_LOST) {
        Application_EnterState(APPLICATION_LINE_LOST);
        Application_ApplySafeState(nowMs);
        return;
    }
    if (status == LINE_CONTROL_INVALID_ARGUMENT) {
        Application_EnterState(APPLICATION_FAULT);
        Application_ApplySafeState(nowMs);
        return;
    }

    steering =
        LineControl_GetSteeringCommand(&gLineControl);
    Chassis_SetOpenLoop(
        &gChassis, gDriveOutput, steering, nowMs);
}

static void Application_RunControlStep(uint32_t nowMs)
{
    uint16_t raw[ADC_SEQUENCE8_COUNT];
    ADC_Status adcStatus;

    adcStatus =
        ADC_ReadSequence8(LINE_ADC_INST, raw);
    if (adcStatus != ADC_STATUS_OK) {
        gAdcErrorCount++;
        Application_EnterState(APPLICATION_ADC_ERROR);
        Application_ApplySafeState(nowMs);
        return;
    }

    if (!LineSensor_ProcessRaw(&gLineSensor, raw)) {
        Application_EnterState(APPLICATION_FAULT);
        Application_ApplySafeState(nowMs);
        return;
    }

    if (gState == APPLICATION_RUNNING) {
        Application_RunLineFollowing(nowMs);
    }
}

static void Application_ProcessOneCommand(
    uint8_t command, uint32_t nowMs)
{
    switch (command) {
        case (uint8_t) 's':
            if (gState == APPLICATION_IDLE) {
                LineControl_Reset(&gLineControl);
                gDriveOutput =
                    APPLICATION_DEFAULT_DRIVE_OUTPUT;
                Application_EnterState(
                    APPLICATION_RUNNING);
            }
            break;

        case (uint8_t) 'x':
            Application_ApplySafeState(nowMs);
            LineControl_Reset(&gLineControl);
            Application_EnterState(APPLICATION_IDLE);
            break;

        case (uint8_t) 't':
            gTelemetryRequested = true;
            break;

        default:
            break;
    }
}

static void Application_ProcessCommands(uint32_t nowMs)
{
    uint8_t command;

    while (HC05_ReadByte(&command)) {
        Application_ProcessOneCommand(command, nowMs);
    }
}

static void Application_SendArray(
    const char *label,
    const uint16_t values[LINE_SENSOR_COUNT])
{
    uint32_t index;

    HC05_SendString(HC05_UART_INST, label);
    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        if (index != 0U) {
            HC05_SendByte(
                HC05_UART_INST, (uint8_t) ',');
        }
        HC05_SendUint32(HC05_UART_INST, values[index]);
    }
    HC05_SendString(HC05_UART_INST, "\r\n");
}

static void Application_PublishTelemetry(uint32_t nowMs)
{
    const LineSensor_Result *line;

    line = LineSensor_GetResult(&gLineSensor);
    HC05_SendString(HC05_UART_INST, "ms=");
    HC05_SendUint32(HC05_UART_INST, nowMs);
    HC05_SendString(HC05_UART_INST, ",state=");
    HC05_SendUint32(HC05_UART_INST, (uint32_t) gState);
    HC05_SendString(HC05_UART_INST, ",position=");
    HC05_SendInt32(
        HC05_UART_INST,
        (line != NULL) ? line->position : 0);
    HC05_SendString(HC05_UART_INST, ",valid=");
    HC05_SendUint32(
        HC05_UART_INST,
        ((line != NULL) && line->valid) ? 1U : 0U);
    HC05_SendString(HC05_UART_INST, ",adcErrors=");
    HC05_SendUint32(HC05_UART_INST, gAdcErrorCount);
    HC05_SendString(HC05_UART_INST, ",missed=");
    HC05_SendUint32(
        HC05_UART_INST, gMissedControlCycles);
    HC05_SendString(HC05_UART_INST, "\r\n");

    if (line != NULL) {
        Application_SendArray("raw=", line->raw);
        Application_SendArray(
            "strength=", line->strength);
    }

    gLastTelemetryMs = nowMs;
}

void Application_Init(void)
{
    const Motor_Config leftMotorConfig = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C0_IDX,
        .in1Port = MOTOR_CONTROL_AIN1_PORT,
        .in1Pin = MOTOR_CONTROL_AIN1_PIN,
        .in2Port = MOTOR_CONTROL_AIN2_PORT,
        .in2Pin = MOTOR_CONTROL_AIN2_PIN,
        .maximumOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .inverted = APPLICATION_LEFT_MOTOR_INVERTED,
        .reversalDelayMs =
            APPLICATION_MOTOR_REVERSAL_DELAY_MS
    };
    const Motor_Config rightMotorConfig = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C1_IDX,
        .in1Port = MOTOR_CONTROL_BIN1_PORT,
        .in1Pin = MOTOR_CONTROL_BIN1_PIN,
        .in2Port = MOTOR_CONTROL_BIN2_PORT,
        .in2Pin = MOTOR_CONTROL_BIN2_PIN,
        .maximumOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .inverted = APPLICATION_RIGHT_MOTOR_INVERTED,
        .reversalDelayMs =
            APPLICATION_MOTOR_REVERSAL_DELAY_MS
    };
    const Servo_Config servoConfig = {
        .pwmTimer = SERVO_PWM_INST,
        .pwmChannel = GPIO_SERVO_PWM_C1_IDX,
        .timerClockHz = SERVO_PWM_INST_CLK_FREQ,
        .minimumPulseUs =
            APPLICATION_SERVO_MINIMUM_PULSE_US,
        .centerPulseUs =
            APPLICATION_SERVO_CENTER_PULSE_US,
        .maximumPulseUs =
            APPLICATION_SERVO_MAXIMUM_PULSE_US,
        .inverted = APPLICATION_SERVO_INVERTED
    };
    const LineSensor_Config lineSensorConfig = {
        .channelMap = APPLICATION_LINE_CHANNEL_MAP,
        .backgroundValue =
            APPLICATION_LINE_BACKGROUND_VALUES,
        .lineValue = APPLICATION_LINE_VALUES,
        .positionWeight =
            APPLICATION_LINE_POSITION_WEIGHTS,
        .minimumCalibrationRange =
            APPLICATION_LINE_MINIMUM_CALIBRATION_RANGE,
        .minimumTotalStrength =
            APPLICATION_LINE_MINIMUM_TOTAL_STRENGTH
    };
    const LineControl_Config lineControlConfig = {
        .steeringPid = {
            .kp = APPLICATION_STEERING_KP,
            .ki = APPLICATION_STEERING_KI,
            .kd = APPLICATION_STEERING_KD,
            .integralMinimum = -1.0f,
            .integralMaximum = 1.0f,
            .outputMinimum = -1.0f,
            .outputMaximum = 1.0f,
            .derivativeFilterCoefficient =
                APPLICATION_DERIVATIVE_FILTER_COEFFICIENT
        },
        .positionFullScale = 3500.0f,
        .maximumSteeringCommand =
            APPLICATION_MAXIMUM_STEERING_COMMAND,
        .steeringInverted = false,
        .maximumInvalidFrames =
            APPLICATION_MAXIMUM_INVALID_FRAMES
    };
    const Chassis_Config chassisConfig = {
        .leftMotor = &gLeftMotor,
        .rightMotor = &gRightMotor,
        .standby = &gMotorStandby,
        .steeringServo = &gSteeringServo,
        .maximumDriveOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .maximumSteeringCommand =
            APPLICATION_MAXIMUM_STEERING_COMMAND,
        .leftOpenLoopScalePermille =
            APPLICATION_LEFT_OPEN_LOOP_SCALE,
        .rightOpenLoopScalePermille =
            APPLICATION_RIGHT_OPEN_LOOP_SCALE
    };
    uint32_t nowMs;
    bool initialized;

    nowMs = SystemTime_GetMs();
    gState = APPLICATION_FAULT;
    gMissedControlCycles = 0U;
    gAdcErrorCount = 0U;
    gLastTelemetryMs = nowMs;
    gTelemetryRequested = false;
    gDriveOutput = 0;

    HC05_ResetReceiver();
    initialized =
        Motor_StandbyInit(
            &gMotorStandby,
            MOTOR_CONTROL_STBY_PORT,
            MOTOR_CONTROL_STBY_PIN) &&
        Motor_Init(
            &gLeftMotor, &leftMotorConfig, nowMs) &&
        Motor_Init(
            &gRightMotor, &rightMotorConfig, nowMs) &&
        Servo_Init(&gSteeringServo, &servoConfig) &&
        LineSensor_Init(
            &gLineSensor, &lineSensorConfig) &&
        LineControl_Init(
            &gLineControl, &lineControlConfig) &&
        Chassis_Init(&gChassis, &chassisConfig) &&
        Chassis_Enable(&gChassis, nowMs);

    if (!initialized) {
        Motor_StandbyDisable(&gMotorStandby);
        return;
    }

    gProcessedControlSequence =
        SystemTime_GetControlSequence();
    Application_EnterState(APPLICATION_IDLE);

    NVIC_ClearPendingIRQ(HC05_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(HC05_UART_INST_INT_IRQN);
    HC05_SendString(
        HC05_UART_INST,
        "ready: s=start, x=brake, t=telemetry\r\n");
}

void Application_Process(void)
{
    uint32_t nowMs;
    uint32_t sequence;
    uint32_t pending;

    nowMs = SystemTime_GetMs();
    Chassis_Process(&gChassis, nowMs);
    Application_ProcessCommands(nowMs);

    sequence = SystemTime_GetControlSequence();
    pending = sequence - gProcessedControlSequence;
    if (pending != 0U) {
        if (pending > 1U) {
            Application_HandleMissedControlCycles(
                pending - 1U);
        }
        gProcessedControlSequence = sequence;
        Application_RunControlStep(nowMs);
    }

    if (gTelemetryRequested) {
        gTelemetryRequested = false;
        Application_PublishTelemetry(nowMs);
    }
}

Application_State Application_GetState(void)
{
    return gState;
}
