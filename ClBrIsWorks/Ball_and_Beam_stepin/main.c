#include <stdint.h>

#include "ti_msp_dl_config.h"

/* Encoder calibration. The recovered MS42CG project says 1000 PPR x4. */
#define ENCODER_COUNTS_PER_REV       (4000LL)
/* From the motor output-shaft viewpoint, CCW must produce a positive count. */
#define QEI_TO_CCW_POSITIVE_SIGN     (1LL)

/* D36A is configured for 16 microsteps: 200 x 16 = 3200 STEP pulses/rev. */
#define MOTOR_STEPS_PER_REV          (3200U)
/* Change to 0 if DIR=1 actually makes the measured angle decrease. */
#define DIR_LEVEL_FOR_POSITIVE_ANGLE (1U)

#define POSITION_TOLERANCE_COUNTS    (2)
#define MAX_ABS_TARGET_MILLIDEG      (90000L)
#define STEP_HIGH_CYCLES             (320U)   /* 10 us at 32 MHz */
#define STEP_LOW_CYCLES              (31680U) /* remainder of a 1 kHz period */
#define DIR_SETUP_CYCLES             (320U)
#define FEEDBACK_CHECK_STEPS         (32U)
#define FEEDBACK_CHECK_COUNTS        (4)
#define NO_FEEDBACK_LIMIT_STEPS      (64U)
#define COMMAND_BUFFER_SIZE          (24U)

typedef enum {
    MOTION_IDLE = 0,
    MOTION_MOVING,
    MOTION_HOLDING,
    MOTION_FAULT
} MotionState;

static uint16_t g_previousRaw;
static int32_t g_accumulatedRaw;
static int32_t g_targetCount;
static int32_t g_targetMillideg;
static int32_t g_motionStartCount;
static uint32_t g_stepsIssued;
static uint32_t g_stepLimit;
static int8_t g_expectedDirection;
static uint8_t g_lastDirLevel;
static uint8_t g_dirInitialized;
static MotionState g_motionState;
static char g_commandBuffer[COMMAND_BUFFER_SIZE];
static uint32_t g_commandLength;

static void uart_putc(char character)
{
    DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, (uint8_t) character);
}

static void uart_puts(const char *text)
{
    while (*text != '\0') {
        uart_putc(*text);
        text++;
    }
}

static void uart_write_u32(uint32_t value)
{
    char digits[10];
    uint32_t index = 0U;

    do {
        digits[index] = (char) ('0' + (value % 10U));
        value /= 10U;
        index++;
    } while (value != 0U);

    while (index != 0U) {
        index--;
        uart_putc(digits[index]);
    }
}

static void uart_write_i32(int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        uart_putc('-');
        magnitude = (uint32_t) (-(value + 1));
        magnitude++;
    } else {
        magnitude = (uint32_t) value;
    }
    uart_write_u32(magnitude);
}

static void uart_write_angle_millideg(int32_t angleMillideg)
{
    uint32_t magnitude;
    uint32_t fraction;

    if (angleMillideg < 0) {
        uart_putc('-');
        magnitude = (uint32_t) (-(angleMillideg + 1));
        magnitude++;
    } else {
        uart_putc('+');
        magnitude = (uint32_t) angleMillideg;
    }

    uart_write_u32(magnitude / 1000U);
    uart_putc('.');
    fraction = magnitude % 1000U;
    uart_putc((char) ('0' + (fraction / 100U)));
    uart_putc((char) ('0' + ((fraction / 10U) % 10U)));
    uart_putc((char) ('0' + (fraction % 10U)));
}

static uint32_t absolute_i32(int32_t value)
{
    if (value >= 0) {
        return (uint32_t) value;
    }
    return (uint32_t) (-(value + 1)) + 1U;
}

static int32_t encoder_update(void)
{
    uint16_t currentRaw;
    int16_t delta;

    currentRaw = (uint16_t) DL_TimerG_getTimerCount(ENCODER_QEI_INST);
    delta = (int16_t) (currentRaw - g_previousRaw);
    g_previousRaw = currentRaw;
    g_accumulatedRaw += (int32_t) delta;

    return (int32_t) ((int64_t) g_accumulatedRaw *
                      QEI_TO_CCW_POSITIVE_SIGN);
}

static int32_t count_to_millideg(int32_t count)
{
    return (int32_t) (((int64_t) count * 360000LL) /
                      ENCODER_COUNTS_PER_REV);
}

static int32_t millideg_to_count(int32_t angleMillideg)
{
    int64_t numerator = (int64_t) angleMillideg * ENCODER_COUNTS_PER_REV;

    if (numerator >= 0) {
        numerator += 180000LL;
    } else {
        numerator -= 180000LL;
    }
    return (int32_t) (numerator / 360000LL);
}

static void motor_disable(void)
{
    DL_GPIO_clearPins(MOTOR_GPIO_PORT,
        MOTOR_GPIO_STEP_PIN | MOTOR_GPIO_EN_PIN);
}

static void motor_enable(void)
{
    DL_GPIO_setPins(MOTOR_GPIO_PORT, MOTOR_GPIO_EN_PIN);
}

static void motor_step(uint8_t directionLevel)
{
    if ((g_dirInitialized == 0U) || (directionLevel != g_lastDirLevel)) {
        if (directionLevel != 0U) {
            DL_GPIO_setPins(MOTOR_GPIO_PORT, MOTOR_GPIO_DIR_PIN);
        } else {
            DL_GPIO_clearPins(MOTOR_GPIO_PORT, MOTOR_GPIO_DIR_PIN);
        }
        g_lastDirLevel = directionLevel;
        g_dirInitialized = 1U;
        delay_cycles(DIR_SETUP_CYCLES);
    }

    DL_GPIO_setPins(MOTOR_GPIO_PORT, MOTOR_GPIO_STEP_PIN);
    delay_cycles(STEP_HIGH_CYCLES);
    DL_GPIO_clearPins(MOTOR_GPIO_PORT, MOTOR_GPIO_STEP_PIN);
    delay_cycles(STEP_LOW_CYCLES);
}

static void print_status(void)
{
    int32_t currentCount = encoder_update();

    uart_puts("count=");
    uart_write_i32(currentCount);
    uart_puts(" angle_deg=");
    uart_write_angle_millideg(count_to_millideg(currentCount));
    uart_puts(" target_deg=");
    uart_write_angle_millideg(g_targetMillideg);
    uart_puts(" state=");
    uart_puts((g_motionState == MOTION_MOVING) ? "MOVING" :
              ((g_motionState == MOTION_HOLDING) ? "HOLDING" :
               ((g_motionState == MOTION_FAULT) ? "FAULT" : "IDLE")));
    uart_puts(" A=");
    uart_write_u32((DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_1) != 0U) ? 1U : 0U);
    uart_puts(" B=");
    uart_write_u32((DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_0) != 0U) ? 1U : 0U);
    uart_puts("\r\n");
}

static uint8_t parse_to_command(const char *text, int32_t *angleMillideg)
{
    const char *cursor = text;
    uint32_t whole = 0U;
    uint32_t fraction = 0U;
    uint32_t fractionDigits = 0U;
    uint8_t negative = 0U;
    uint8_t haveDigit = 0U;

    if (!((cursor[0] == 'T' || cursor[0] == 't') &&
          (cursor[1] == 'O' || cursor[1] == 'o'))) {
        return 0U;
    }
    cursor += 2;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == '+' || *cursor == '-') {
        negative = (*cursor == '-') ? 1U : 0U;
        cursor++;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        haveDigit = 1U;
        if (whole > 10000U) return 0U;
        whole = whole * 10U + (uint32_t) (*cursor - '0');
        cursor++;
    }
    if (*cursor == '.') {
        cursor++;
        while (*cursor >= '0' && *cursor <= '9') {
            if (fractionDigits < 3U) {
                fraction = fraction * 10U + (uint32_t) (*cursor - '0');
                fractionDigits++;
            } else if (*cursor != '0') {
                return 0U;
            }
            cursor++;
        }
    }
    while (fractionDigits < 3U) {
        fraction *= 10U;
        fractionDigits++;
    }
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (haveDigit == 0U || *cursor != '\0') return 0U;

    if (whole > 2147483U) return 0U;
    *angleMillideg = (int32_t) (whole * 1000U + fraction);
    if (negative != 0U) *angleMillideg = -*angleMillideg;
    return 1U;
}

static void start_position_move(int32_t targetMillideg)
{
    int32_t currentCount;
    int32_t error;
    uint32_t expectedSteps;

    if (targetMillideg > MAX_ABS_TARGET_MILLIDEG ||
        targetMillideg < -MAX_ABS_TARGET_MILLIDEG) {
        uart_puts("ERR range: allowed -90.000 to +90.000 deg\r\n");
        return;
    }

    currentCount = encoder_update();
    g_targetMillideg = targetMillideg;
    g_targetCount = millideg_to_count(targetMillideg);
    error = g_targetCount - currentCount;
    g_motionStartCount = currentCount;
    g_stepsIssued = 0U;
    g_expectedDirection = (error >= 0) ? 1 : -1;
    expectedSteps = (absolute_i32(error) * MOTOR_STEPS_PER_REV +
                     (uint32_t) ENCODER_COUNTS_PER_REV - 1U) /
                    (uint32_t) ENCODER_COUNTS_PER_REV;
    g_stepLimit = expectedSteps * 2U + 100U;
    g_motionState = (absolute_i32(error) <=
                     (uint32_t) POSITION_TOLERANCE_COUNTS) ?
                    MOTION_HOLDING : MOTION_MOVING;
    motor_enable();

    uart_puts("OK target_deg=");
    uart_write_angle_millideg(g_targetMillideg);
    uart_puts(" target_count=");
    uart_write_i32(g_targetCount);
    uart_puts("\r\n");
    if (g_motionState == MOTION_HOLDING) print_status();
}

static void handle_command(char *command)
{
    int32_t targetMillideg;

    if (parse_to_command(command, &targetMillideg) != 0U) {
        start_position_move(targetMillideg);
    } else if ((command[0] == '?' && command[1] == '\0') ||
               ((command[0] == 'S' || command[0] == 's') &&
                command[1] == '\0')) {
        if (command[0] == '?') {
            print_status();
        } else {
            g_motionState = MOTION_IDLE;
            motor_disable();
            uart_puts("OK stopped and driver disabled\r\n");
        }
    } else {
        uart_puts("ERR command; use To30, To-30, ?, or S, then press Enter\r\n");
    }
}

static void poll_uart(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(DEBUG_UART_INST)) {
        char character = (char) DL_UART_Main_receiveData(DEBUG_UART_INST);

        if (character == '\r' || character == '\n') {
            if (g_commandLength != 0U) {
                g_commandBuffer[g_commandLength] = '\0';
                handle_command(g_commandBuffer);
                g_commandLength = 0U;
            }
        } else if (g_commandLength < (COMMAND_BUFFER_SIZE - 1U)) {
            g_commandBuffer[g_commandLength] = character;
            g_commandLength++;
        } else {
            g_commandLength = 0U;
            uart_puts("ERR command too long\r\n");
        }
    }
}

static void position_control_process(void)
{
    int32_t currentCount;
    int32_t error;
    int32_t movement;
    uint8_t directionLevel;

    if (g_motionState != MOTION_MOVING &&
        g_motionState != MOTION_HOLDING) return;

    currentCount = encoder_update();
    error = g_targetCount - currentCount;
    if (absolute_i32(error) <= (uint32_t) POSITION_TOLERANCE_COUNTS) {
        if (g_motionState == MOTION_MOVING) {
            g_motionState = MOTION_HOLDING;
            DL_GPIO_clearPins(MOTOR_GPIO_PORT, MOTOR_GPIO_STEP_PIN);
            uart_puts("REACHED and HOLDING ");
            print_status();
        }
        return;
    }

    if (g_motionState == MOTION_HOLDING) {
        uint32_t expectedSteps;

        g_motionState = MOTION_MOVING;
        g_motionStartCount = currentCount;
        g_stepsIssued = 0U;
        g_expectedDirection = (error > 0) ? 1 : -1;
        expectedSteps = (absolute_i32(error) * MOTOR_STEPS_PER_REV +
                         (uint32_t) ENCODER_COUNTS_PER_REV - 1U) /
                        (uint32_t) ENCODER_COUNTS_PER_REV;
        g_stepLimit = expectedSteps * 2U + 100U;
    }

    if (g_stepsIssued >= g_stepLimit) {
        g_motionState = MOTION_FAULT;
        motor_disable();
        uart_puts("FAULT no encoder feedback or mechanism blocked\r\n");
        print_status();
        return;
    }

    directionLevel = (error > 0) ? DIR_LEVEL_FOR_POSITIVE_ANGLE :
                                   (uint8_t) !DIR_LEVEL_FOR_POSITIVE_ANGLE;
    motor_step(directionLevel);
    g_stepsIssued++;

    if (g_stepsIssued >= FEEDBACK_CHECK_STEPS) {
        currentCount = encoder_update();
        movement = currentCount - g_motionStartCount;
        if (absolute_i32(movement) >= FEEDBACK_CHECK_COUNTS &&
            ((g_expectedDirection > 0 && movement < 0) ||
             (g_expectedDirection < 0 && movement > 0))) {
            g_motionState = MOTION_FAULT;
            motor_disable();
            uart_puts("FAULT feedback direction reversed; change DIR_LEVEL_FOR_POSITIVE_ANGLE\r\n");
            print_status();
        } else if (g_stepsIssued >= NO_FEEDBACK_LIMIT_STEPS &&
                   absolute_i32(movement) < FEEDBACK_CHECK_COUNTS) {
            g_motionState = MOTION_FAULT;
            motor_disable();
            uart_puts("FAULT encoder did not move after 64 STEP pulses\r\n");
            print_status();
        }
    }
}

int main(void)
{
    SYSCFG_DL_init();

    motor_disable();
    DL_TimerG_setTimerCount(ENCODER_QEI_INST, 0U);
    DL_TimerG_startCounter(ENCODER_QEI_INST);
    g_previousRaw = (uint16_t) DL_TimerG_getTimerCount(ENCODER_QEI_INST);
    g_accumulatedRaw = 0;
    g_targetCount = 0;
    g_targetMillideg = 0;
    g_motionState = MOTION_IDLE;
    g_commandLength = 0U;
    g_dirInitialized = 0U;

    uart_puts("\r\nMS42CG position control ready\r\n");
    uart_puts("Power-on position=0 deg, CCW=positive, CW=negative\r\n");
    uart_puts("Position remains closed-loop: external displacement is corrected automatically.\r\n");
    uart_puts("Commands: To30, To-30, To12.5, ?, S (press Enter)\r\n");

    while (1) {
        poll_uart();
        position_control_process();
        if (g_motionState != MOTION_MOVING) {
            (void) encoder_update();
        }
    }
}
