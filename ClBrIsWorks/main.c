#include "adc.h"
#include "chassis.h"
#include "system_time.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>

#define LINE_PRINT_PERIOD_MS (100U)

#define MOTOR_MAXIMUM_OUTPUT (200U)
#define WHEEL_STRAIGHT_OUTPUT (170)
#define WHEEL_CORRECTION_SLOW_OUTPUT (140)
#define WHEEL_CORRECTION_FAST_OUTPUT (180)
#define WHEEL_SHARP_SLOW_OUTPUT (110)
#define WHEEL_SHARP_FAST_OUTPUT (190)
#define WHEEL_SEARCH_SLOW_OUTPUT (110)
#define WHEEL_SEARCH_FAST_OUTPUT (170)

static uint16_t adc_line[ADC_SEQUENCE5_COUNT];
static uint8_t line_state[ADC_SEQUENCE5_COUNT];
static Motor left_motor;
static Motor right_motor;
static Chassis chassis;
static int8_t last_line_direction;

/*
 * 五路阈值分别取白底和黑线实测值的中点。
 * ADC 值大于等于对应阈值时判定为黑线。
 */
static const uint16_t line_threshold[ADC_SEQUENCE5_COUNT] = {
    330U, 295U, 290U, 300U, 500U
};

static ADC_Status read_line_sensor(void)
{
    return ADC_ReadSequence5(LINE_ADC_INST, adc_line);
}

static void update_line_state(void)
{
    uint32_t index;

    for (index = 0U; index < ADC_SEQUENCE5_COUNT; index++) {
        line_state[index] =
            (adc_line[index] >= line_threshold[index]) ? 1U : 0U;
    }
}

static bool init_chassis(void)
{
    const Motor_Config left_motor_config = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C1_IDX,
        .in1Port = MOTOR_CONTROL_BIN1_PORT,
        .in1Pin = MOTOR_CONTROL_BIN1_PIN,
        .in2Port = MOTOR_CONTROL_BIN2_PORT,
        .in2Pin = MOTOR_CONTROL_BIN2_PIN,
        .maximumOutput = MOTOR_MAXIMUM_OUTPUT,
        .inverted = false,
        .reversalDelayMs = 5U
    };
    const Motor_Config right_motor_config = {
        .pwmTimer = MOTOR_PWM_INST,
        .pwmChannel = GPIO_MOTOR_PWM_C0_IDX,
        .in1Port = MOTOR_CONTROL_AIN1_PORT,
        .in1Pin = MOTOR_CONTROL_AIN1_PIN,
        .in2Port = MOTOR_CONTROL_AIN2_PORT,
        .in2Pin = MOTOR_CONTROL_AIN2_PIN,
        .maximumOutput = MOTOR_MAXIMUM_OUTPUT,
        .inverted = false,
        .reversalDelayMs = 5U
    };
    const Chassis_Config chassis_config = {
        .leftMotor = &left_motor,
        .rightMotor = &right_motor,
        .standby = NULL,
        .maximumDriveOutput = MOTOR_MAXIMUM_OUTPUT,
        .maximumTurnOutput = MOTOR_MAXIMUM_OUTPUT,
        .leftOpenLoopScalePermille = 1000U,
        .rightOpenLoopScalePermille = 1000U
    };
    uint32_t now_ms = SystemTime_GetMs();

    return Motor_Init(
               &left_motor, &left_motor_config, now_ms) &&
        Motor_Init(
               &right_motor, &right_motor_config, now_ms) &&
        Chassis_Init(&chassis, &chassis_config) &&
        Chassis_Enable(&chassis, now_ms);
}

/*
 * line_state 从左到右排列。error > 0 表示黑线在车体左侧，
 * error < 0 表示黑线在车体右侧。
 */
static void follow_line(uint32_t now_ms)
{
    int32_t error;
    uint32_t active_count;
    int16_t left_output;
    int16_t right_output;

    error =
        2 * (int32_t) line_state[0] +
        (int32_t) line_state[1] -
        (int32_t) line_state[3] -
        2 * (int32_t) line_state[4];
    active_count =
        (uint32_t) line_state[0] +
        (uint32_t) line_state[1] +
        (uint32_t) line_state[2] +
        (uint32_t) line_state[3] +
        (uint32_t) line_state[4];

    if (active_count == 0U) {
        if (last_line_direction > 0) {
            Chassis_SetWheelOutputs(
                &chassis,
                WHEEL_SEARCH_SLOW_OUTPUT,
                WHEEL_SEARCH_FAST_OUTPUT,
                now_ms);
        } else if (last_line_direction < 0) {
            Chassis_SetWheelOutputs(
                &chassis,
                WHEEL_SEARCH_FAST_OUTPUT,
                WHEEL_SEARCH_SLOW_OUTPUT,
                now_ms);
        } else {
            Chassis_Brake(&chassis, now_ms);
        }
        return;
    }

    if (error > 0) {
        last_line_direction = 1;
    } else if (error < 0) {
        last_line_direction = -1;
    }

    if (error >= 2) {
        left_output = WHEEL_SHARP_SLOW_OUTPUT;
        right_output = WHEEL_SHARP_FAST_OUTPUT;
    } else if (error == 1) {
        left_output = WHEEL_CORRECTION_SLOW_OUTPUT;
        right_output = WHEEL_CORRECTION_FAST_OUTPUT;
    } else if (error == -1) {
        left_output = WHEEL_CORRECTION_FAST_OUTPUT;
        right_output = WHEEL_CORRECTION_SLOW_OUTPUT;
    } else if (error <= -2) {
        left_output = WHEEL_SHARP_FAST_OUTPUT;
        right_output = WHEEL_SHARP_SLOW_OUTPUT;
    } else {
        left_output = WHEEL_STRAIGHT_OUTPUT;
        right_output = WHEEL_STRAIGHT_OUTPUT;
    }

    Chassis_SetWheelOutputs(
        &chassis, left_output, right_output, now_ms);
}

static void print_line_state(void)
{
    printf("%u %u %u %u %u\n",
           (unsigned int) line_state[0],
           (unsigned int) line_state[1],
           (unsigned int) line_state[2],
           (unsigned int) line_state[3],
           (unsigned int) line_state[4]);
    printf("%u %u %u %u %u\n",
           (unsigned int) adc_line[0],
           (unsigned int) adc_line[1],
           (unsigned int) adc_line[2],
           (unsigned int) adc_line[3],
           (unsigned int) adc_line[4]);
}

int main(void)
{
    uint32_t last_control_sequence;
    uint32_t last_print_ms;

    SYSCFG_DL_init();
    printf("04debug\n");

    if (!init_chassis()) {
        printf("chassis init failed\n");
        for (;;) {
            __WFI();
        }
    }

    last_control_sequence = SystemTime_GetControlSequence();
    last_print_ms = SystemTime_GetMs();

    for (;;) {
        uint32_t now_ms = SystemTime_GetMs();
        uint32_t control_sequence =
            SystemTime_GetControlSequence();

        Chassis_Process(&chassis, now_ms);

        if (control_sequence != last_control_sequence) {
            last_control_sequence = control_sequence;

            if (read_line_sensor() == ADC_STATUS_OK) {
                update_line_state();
                follow_line(now_ms);
            } else {
                Chassis_Brake(&chassis, now_ms);
            }
        }

        if (SystemTime_ElapsedMs(
                now_ms, last_print_ms) >=
            LINE_PRINT_PERIOD_MS) {
            last_print_ms = now_ms;
            print_line_state();
        }

        __WFI();
    }
}

void SysTick_Handler(void)
{
    SystemTime_On1msTick();
}
