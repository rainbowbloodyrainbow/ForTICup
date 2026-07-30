#include "application.h"
#include "application_policy.h"

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
#include "system_time.h"
#include "ti_msp_dl_config.h"

#define APPLICATION_CONTROL_DT_SECONDS (0.01f)

static Application_State gState;
static uint32_t gProcessedControlSequence;
static uint32_t gMissedControlCycles;
static uint32_t gAdcErrorCount;
static uint32_t gLastTelemetryMs;
static bool gTelemetryRequested;
static bool gRawStreamingEnabled;
static int16_t gDriveOutput;

static Motor gLeftMotor;
static Motor gRightMotor;
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
    int16_t turn;

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

    turn =
        LineControl_GetTurnCommand(&gLineControl);
    turn = ApplicationPolicy_LimitTurnForForwardDrive(
        gDriveOutput, turn);
    Chassis_SetDriveTurn(
        &gChassis, gDriveOutput, turn, nowMs);
}

static void Application_RunControlStep(uint32_t nowMs)
{
    uint16_t raw[ADC_SEQUENCE5_COUNT];
    ADC_Status adcStatus;

    adcStatus =
        ADC_ReadSequence5(LINE_ADC_INST, raw);
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

        case (uint8_t) 'v':
            gRawStreamingEnabled =
                !gRawStreamingEnabled;
            gLastTelemetryMs = nowMs;
            HC05_SendString(
                HC05_UART_INST,
                gRawStreamingEnabled ?
                "raw-stream=on\r\n" :
                "raw-stream=off\r\n");
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
    HC05_SendString(HC05_UART_INST, ",drive=");
    HC05_SendInt32(
        HC05_UART_INST,
        Chassis_GetDriveOutput(&gChassis));
    HC05_SendString(HC05_UART_INST, ",turn=");
    HC05_SendInt32(
        HC05_UART_INST,
        Chassis_GetTurnOutput(&gChassis));
    HC05_SendString(HC05_UART_INST, ",left=");
    HC05_SendInt32(
        HC05_UART_INST,
        Chassis_GetLeftOutput(&gChassis));
    HC05_SendString(HC05_UART_INST, ",right=");
    HC05_SendInt32(
        HC05_UART_INST,
        Chassis_GetRightOutput(&gChassis));
    HC05_SendString(HC05_UART_INST, "\r\n");

    if (line != NULL) {
        Application_SendArray("raw=", line->raw);
        Application_SendArray(
            "strength=", line->strength);
    }

    gLastTelemetryMs = nowMs;
}

static void Application_PublishRawValues(uint32_t nowMs)
{
    const LineSensor_Result *line;

    line = LineSensor_GetResult(&gLineSensor);
    if (line != NULL) {
        Application_SendArray("raw=", line->raw);
    }
    gLastTelemetryMs = nowMs;
}

void Application_Init(void)
{
    const Motor_Config leftMotorConfig = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C1_IDX,
        .in1Port = MOTOR_CONTROL_BIN1_PORT,
        .in1Pin = MOTOR_CONTROL_BIN1_PIN,
        .in2Port = MOTOR_CONTROL_BIN2_PORT,
        .in2Pin = MOTOR_CONTROL_BIN2_PIN,
        .maximumOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .inverted = APPLICATION_LEFT_MOTOR_INVERTED,
        .reversalDelayMs =
            APPLICATION_MOTOR_REVERSAL_DELAY_MS
    };
    const Motor_Config rightMotorConfig = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C0_IDX,
        .in1Port = MOTOR_CONTROL_AIN1_PORT,
        .in1Pin = MOTOR_CONTROL_AIN1_PIN,
        .in2Port = MOTOR_CONTROL_AIN2_PORT,
        .in2Pin = MOTOR_CONTROL_AIN2_PIN,
        .maximumOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .inverted = APPLICATION_RIGHT_MOTOR_INVERTED,
        .reversalDelayMs =
            APPLICATION_MOTOR_REVERSAL_DELAY_MS
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
        .turnPid = {
            .kp = APPLICATION_TURN_KP,
            .ki = APPLICATION_TURN_KI,
            .kd = APPLICATION_TURN_KD,
            .integralMinimum = -1.0f,
            .integralMaximum = 1.0f,
            .outputMinimum = -1.0f,
            .outputMaximum = 1.0f,
            .derivativeFilterCoefficient =
                APPLICATION_DERIVATIVE_FILTER_COEFFICIENT
        },
        .positionFullScale = 2000.0f,
        .maximumTurnCommand =
            APPLICATION_MAXIMUM_TURN_OUTPUT,
        .turnInverted = APPLICATION_TURN_INVERTED,
        .maximumInvalidFrames =
            APPLICATION_MAXIMUM_INVALID_FRAMES
    };
    const Chassis_Config chassisConfig = {
        .leftMotor = &gLeftMotor,
        .rightMotor = &gRightMotor,
        .standby = NULL,
        .maximumDriveOutput =
            APPLICATION_MOTOR_MAXIMUM_OUTPUT,
        .maximumTurnOutput =
            APPLICATION_MAXIMUM_TURN_OUTPUT,
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
    gRawStreamingEnabled = false;
    gDriveOutput = 0;

    HC05_ResetReceiver();
    initialized =
        Motor_Init(
            &gLeftMotor, &leftMotorConfig, nowMs) &&
        Motor_Init(
            &gRightMotor, &rightMotorConfig, nowMs) &&
        LineSensor_Init(
            &gLineSensor, &lineSensorConfig) &&
        LineControl_Init(
            &gLineControl, &lineControlConfig) &&
        Chassis_Init(&gChassis, &chassisConfig) &&
        Chassis_Enable(&gChassis, nowMs);

    if (!initialized) {
        Chassis_Disable(&gChassis, nowMs);
        return;
    }

    gProcessedControlSequence =
        SystemTime_GetControlSequence();
    Application_EnterState(APPLICATION_IDLE);

    NVIC_ClearPendingIRQ(HC05_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(HC05_UART_INST_INT_IRQN);
    HC05_SendString(
        HC05_UART_INST,
        "ready: s=start, x=brake, t=telemetry, "
        "v=raw-stream\r\n");
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

    if (gRawStreamingEnabled &&
        (SystemTime_ElapsedMs(
            nowMs, gLastTelemetryMs) >=
            APPLICATION_RAW_STREAM_PERIOD_MS)) {
        Application_PublishRawValues(nowMs);
    }
}

Application_State Application_GetState(void)
{
    return gState;
}
