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
#define DIR_SETUP_CYCLES             (320U)
#define CONTROL_PERIOD_MS            (10U)
#define STEP_SCHEDULER_HZ            (1000U)
#define MAX_STEP_RATE_PPS            (1000)

/* Position P loop: position error[count] -> target speed[count/s]. */
#define POSITION_KP_NUM              (6)
#define POSITION_KP_DEN              (1)
#define MAX_TARGET_SPEED_COUNTS_S    (1000)

/* Speed PI loop: speed error[count/s] -> STEP rate[pulse/s]. */
#define SPEED_FILTER_DIVISOR         (4)
#define SPEED_KP_NUM                 (1)
#define SPEED_KP_DEN                 (4)
#define SPEED_KI_DIVISOR             (500)
#define SPEED_INTEGRAL_LIMIT         (150000)

#define FEEDBACK_CHECK_STEPS         (32U)
#define FEEDBACK_CHECK_COUNTS        (4)
#define NO_FEEDBACK_LIMIT_STEPS      (64U)
#define COMMAND_BUFFER_SIZE          (24U)
#define OPENMV_BUFFER_SIZE           (12U)
#define VISION_CENTER_X              (180U)
#define VISION_DEADBAND_PIXELS       (2)
#define VISION_POS_REF_MILLIDEG      (30000L)
#define VISION_NEG_REF_MAG_MILLIDEG  (40000L)
#define VISION_MIN_TARGET_MILLIDEG   (-60000L)
#define VISION_MAX_TARGET_MILLIDEG   (45000L)
#define BALL_TILT_U_LIMIT_MILLI      (1500)

/* Ball position P loop: position error[pixel] -> target speed[pixel/s]. */
#define BALL_POSITION_KP_NUM         (4)
#define BALL_POSITION_KP_DEN         (1)
#define BALL_MAX_TARGET_SPEED_PX_S   (400)

/* Ball speed PI loop: speed error[pixel/s] -> symmetric tilt demand u. */
#define BALL_SPEED_FILTER_DIVISOR    (4)
#define BALL_SPEED_KP_U_NUM          (5)
#define BALL_SPEED_KP_U_DEN          (2)
#define BALL_SPEED_KI_U_PER_PIXEL    (1)
#define BALL_SPEED_I_LIMIT_PIXEL_MS  (300000)
#define BALL_STOP_SPEED_BAND_PX_S    (5)
#define VISION_STATUS_EVERY_FRAMES   (10U)

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
static int32_t g_targetSpeedCountsPerS;
static int32_t g_measuredSpeedCountsPerS;
static int32_t g_commandStepRatePps;
static int32_t g_speedIntegralError;
static int32_t g_lastControlCount;
static uint32_t g_lastControlMs;
static uint32_t g_lastStepSchedulerMs;
static uint32_t g_stepPhaseAccumulator;
static int32_t g_feedbackStartCount;
static uint32_t g_feedbackPulseCount;
static int8_t g_feedbackExpectedDirection;
static uint8_t g_lastDirLevel;
static uint8_t g_dirInitialized;
static MotionState g_motionState;
static char g_commandBuffer[COMMAND_BUFFER_SIZE];
static uint32_t g_commandLength;
static char g_openmvBuffer[OPENMV_BUFFER_SIZE];
static uint32_t g_openmvLength;
static uint8_t g_visionEnabled;
static uint32_t g_visionStatusFrameCount;
static uint32_t g_ballLatestX;
static uint32_t g_ballPreviousX;
static int32_t g_ballTargetSpeedPixelsPerS;
static int32_t g_ballMeasuredSpeedPixelsPerS;
static int32_t g_ballSpeedIntegralPixelMs;
static uint32_t g_ballPreviousMs;
static uint8_t g_ballControllerInitialized;
static volatile uint32_t g_milliseconds;

void SysTick_Handler(void)
{
    g_milliseconds++;
}

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

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void reset_ball_controller(void)
{
    g_ballLatestX = VISION_CENTER_X;
    g_ballPreviousX = VISION_CENTER_X;
    g_ballTargetSpeedPixelsPerS = 0;
    g_ballMeasuredSpeedPixelsPerS = 0;
    g_ballSpeedIntegralPixelMs = 0;
    g_ballPreviousMs = 0U;
    g_ballControllerInitialized = 0U;
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
    g_commandStepRatePps = 0;
    g_stepPhaseAccumulator = 0U;
    DL_GPIO_clearPins(MOTOR_GPIO_PORT,
        MOTOR_GPIO_STEP_PIN | MOTOR_GPIO_EN_PIN);
}

static void motor_enable(void)
{
    DL_GPIO_setPins(MOTOR_GPIO_PORT, MOTOR_GPIO_EN_PIN);
}

static void motor_emit_step(uint8_t directionLevel)
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
    uart_puts(" speed_cps=");
    uart_write_i32(g_measuredSpeedCountsPerS);
    uart_puts(" speed_target_cps=");
    uart_write_i32(g_targetSpeedCountsPerS);
    uart_puts(" step_pps=");
    uart_write_i32(g_commandStepRatePps);
    uart_puts(" state=");
    uart_puts((g_motionState == MOTION_MOVING) ? "MOVING" :
              ((g_motionState == MOTION_HOLDING) ? "HOLDING" :
               ((g_motionState == MOTION_FAULT) ? "FAULT" : "IDLE")));
    uart_puts(" vision=");
    uart_puts((g_visionEnabled != 0U) ? "ON" : "OFF");
    uart_puts(" ball=");
    uart_puts((g_ballControllerInitialized != 0U) ? "VALID" : "NO_DATA");
    uart_puts(" ball_x=");
    uart_write_u32(g_ballLatestX);
    uart_puts(" ball_v_px_s=");
    uart_write_i32(g_ballMeasuredSpeedPixelsPerS);
    uart_puts(" ball_v_target_px_s=");
    uart_write_i32(g_ballTargetSpeedPixelsPerS);
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

    if (targetMillideg > MAX_ABS_TARGET_MILLIDEG ||
        targetMillideg < -MAX_ABS_TARGET_MILLIDEG) {
        uart_puts("ERR range: allowed -90.000 to +90.000 deg\r\n");
        return;
    }

    currentCount = encoder_update();
    g_targetMillideg = targetMillideg;
    g_targetCount = millideg_to_count(targetMillideg);
    error = g_targetCount - currentCount;
    g_targetSpeedCountsPerS = 0;
    g_measuredSpeedCountsPerS = 0;
    g_commandStepRatePps = 0;
    g_speedIntegralError = 0;
    g_lastControlCount = currentCount;
    g_lastControlMs = g_milliseconds;
    g_lastStepSchedulerMs = g_milliseconds;
    g_stepPhaseAccumulator = 0U;
    g_feedbackStartCount = currentCount;
    g_feedbackPulseCount = 0U;
    g_feedbackExpectedDirection = 0;
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

static void update_vision_position_target(int32_t targetMillideg)
{
    int32_t currentCount;
    int32_t previousError;
    int32_t newError;
    uint8_t controllerWasInactive;

    if (g_motionState == MOTION_FAULT) return;

    currentCount = encoder_update();
    previousError = g_targetCount - currentCount;
    g_targetMillideg = clamp_i32(
        targetMillideg,
        VISION_MIN_TARGET_MILLIDEG,
        VISION_MAX_TARGET_MILLIDEG);
    g_targetCount = millideg_to_count(g_targetMillideg);
    newError = g_targetCount - currentCount;
    controllerWasInactive = (g_motionState == MOTION_IDLE) ? 1U : 0U;

    if (controllerWasInactive != 0U) {
        g_measuredSpeedCountsPerS = 0;
        g_lastControlCount = currentCount;
        g_lastControlMs = g_milliseconds;
        g_lastStepSchedulerMs = g_milliseconds;
    }
    if (controllerWasInactive != 0U ||
        (previousError > 0 && newError < 0) ||
        (previousError < 0 && newError > 0)) {
        g_targetSpeedCountsPerS = 0;
        g_commandStepRatePps = 0;
        g_speedIntegralError = 0;
        g_stepPhaseAccumulator = 0U;
        g_feedbackStartCount = currentCount;
        g_feedbackPulseCount = 0U;
        g_feedbackExpectedDirection = 0;
    }

    g_motionState = (absolute_i32(newError) <=
                     (uint32_t) POSITION_TOLERANCE_COUNTS) ?
                    MOTION_HOLDING : MOTION_MOVING;
    motor_enable();
}

static void handle_command(char *command)
{
    int32_t targetMillideg;

    if (parse_to_command(command, &targetMillideg) != 0U) {
        g_visionEnabled = 0U;
        reset_ball_controller();
        start_position_move(targetMillideg);
    } else if ((command[0] == '?' && command[1] == '\0') ||
               ((command[0] == 'S' || command[0] == 's') &&
                command[1] == '\0')) {
        if (command[0] == '?') {
            print_status();
        } else {
            g_visionEnabled = 0U;
            reset_ball_controller();
            g_motionState = MOTION_IDLE;
            motor_disable();
            uart_puts("OK stopped; driver disabled and vision OFF\r\n");
        }
    } else if ((command[0] == 'V' || command[0] == 'v') &&
               command[1] == '\0') {
        motor_disable();
        g_motionState = MOTION_IDLE;
        g_visionEnabled = 1U;
        g_visionStatusFrameCount = 0U;
        reset_ball_controller();
        uart_puts("OK vision control ON\r\n");
    } else {
        uart_puts("ERR command; use V, To30, To-30, ?, or S, then press Enter\r\n");
    }
}

static uint8_t parse_openmv_x(const char *text, uint32_t *x)
{
    const char *cursor = text;
    uint32_t value = 0U;
    uint8_t haveDigit = 0U;

    if (*cursor != 'X' && *cursor != 'x') return 0U;
    cursor++;
    while (*cursor >= '0' && *cursor <= '9') {
        haveDigit = 1U;
        value = value * 10U + (uint32_t) (*cursor - '0');
        if (value > 319U) return 0U;
        cursor++;
    }
    if (haveDigit == 0U || *cursor != '\0') return 0U;
    *x = value;
    return 1U;
}

static void print_ball_cascade_status(
    uint32_t x,
    int32_t positionError,
    int32_t speedError,
    int32_t speedProportionalUMilli,
    int32_t speedIntegralUMilli,
    int32_t tiltCommandUMilli,
    int32_t targetMillideg)
{
    int32_t currentCount = encoder_update();

    uart_puts("BALL_CASCADE x=");
    uart_write_u32(x);
    uart_puts(" pos_err_px=");
    uart_write_i32(positionError);
    uart_puts(" ball_v_target_px_s=");
    uart_write_i32(g_ballTargetSpeedPixelsPerS);
    uart_puts(" ball_v_px_s=");
    uart_write_i32(g_ballMeasuredSpeedPixelsPerS);
    uart_puts(" speed_err_px_s=");
    uart_write_i32(speedError);
    uart_puts(" speed_p_u_milli=");
    uart_write_i32(speedProportionalUMilli);
    uart_puts(" speed_i_u_milli=");
    uart_write_i32(speedIntegralUMilli);
    uart_puts(" u_milli=");
    uart_write_i32(tiltCommandUMilli);
    uart_puts(" target_deg=");
    uart_write_angle_millideg(targetMillideg);
    uart_puts(" current_deg=");
    uart_write_angle_millideg(count_to_millideg(currentCount));
    uart_puts(" step_pps=");
    uart_write_i32(g_commandStepRatePps);
    uart_puts("\r\n");
}

static int32_t vision_tilt_to_motor_millideg(int32_t tiltCommandUMilli)
{
    int64_t motorMillideg;

    if (tiltCommandUMilli >= 0) {
        motorMillideg =
            ((int64_t) tiltCommandUMilli * VISION_POS_REF_MILLIDEG) /
            1000LL;
    } else {
        motorMillideg =
            ((int64_t) tiltCommandUMilli * VISION_NEG_REF_MAG_MILLIDEG) /
            1000LL;
    }
    return clamp_i32(
        (int32_t) motorMillideg,
        VISION_MIN_TARGET_MILLIDEG,
        VISION_MAX_TARGET_MILLIDEG);
}

static int32_t ball_cascade_calculate(
    uint32_t x,
    int32_t *positionErrorOut,
    int32_t *speedErrorOut,
    int32_t *speedProportionalOut,
    int32_t *speedIntegralOut,
    int32_t *tiltCommandOut)
{
    uint32_t now = g_milliseconds;
    uint32_t elapsedMs = 0U;
    int32_t positionError = (int32_t) VISION_CENTER_X - (int32_t) x;
    int32_t rawBallSpeed = 0;
    int32_t speedError;
    int32_t speedProportionalUMilli;
    int32_t speedIntegralUMilli;
    int32_t tiltCommandUMilli;
    int32_t unclampedTiltUMilli;
    int64_t candidateIntegral;
    uint8_t allowIntegration;

    if (absolute_i32(positionError) <= VISION_DEADBAND_PIXELS) {
        positionError = 0;
    }

    g_ballLatestX = x;
    if (g_ballControllerInitialized == 0U) {
        g_ballControllerInitialized = 1U;
        g_ballPreviousX = x;
        g_ballPreviousMs = now;
        g_ballMeasuredSpeedPixelsPerS = 0;
        g_ballSpeedIntegralPixelMs = 0;
    } else {
        elapsedMs = now - g_ballPreviousMs;
        if (elapsedMs != 0U) {
            if (elapsedMs > 200U) {
                g_ballMeasuredSpeedPixelsPerS = 0;
                g_ballSpeedIntegralPixelMs = 0;
            } else {
                rawBallSpeed = (int32_t) (
                    ((int64_t) ((int32_t) x - (int32_t) g_ballPreviousX) *
                     1000LL) / elapsedMs);
                g_ballMeasuredSpeedPixelsPerS +=
                    (rawBallSpeed - g_ballMeasuredSpeedPixelsPerS) /
                    BALL_SPEED_FILTER_DIVISOR;
            }
            g_ballPreviousX = x;
            g_ballPreviousMs = now;
        }
    }

    g_ballTargetSpeedPixelsPerS = (int32_t) (
        ((int64_t) positionError * BALL_POSITION_KP_NUM) /
        BALL_POSITION_KP_DEN);
    g_ballTargetSpeedPixelsPerS = clamp_i32(
        g_ballTargetSpeedPixelsPerS,
        -BALL_MAX_TARGET_SPEED_PX_S,
        BALL_MAX_TARGET_SPEED_PX_S);

    speedError = g_ballTargetSpeedPixelsPerS -
                 g_ballMeasuredSpeedPixelsPerS;
    speedProportionalUMilli = (int32_t) (
        ((int64_t) speedError * BALL_SPEED_KP_U_NUM) /
        BALL_SPEED_KP_U_DEN);

    speedIntegralUMilli = (int32_t) (
        ((int64_t) g_ballSpeedIntegralPixelMs *
         BALL_SPEED_KI_U_PER_PIXEL) / 1000LL);
    unclampedTiltUMilli = speedProportionalUMilli + speedIntegralUMilli;

    /* Conditional integration prevents the ball-speed PI from winding up. */
    allowIntegration =
        ((unclampedTiltUMilli < BALL_TILT_U_LIMIT_MILLI &&
          unclampedTiltUMilli > -BALL_TILT_U_LIMIT_MILLI) ||
         (unclampedTiltUMilli >= BALL_TILT_U_LIMIT_MILLI &&
          speedError < 0) ||
         (unclampedTiltUMilli <= -BALL_TILT_U_LIMIT_MILLI &&
          speedError > 0)) ? 1U : 0U;
    if (allowIntegration != 0U && elapsedMs != 0U && elapsedMs <= 200U) {
        candidateIntegral = (int64_t) g_ballSpeedIntegralPixelMs +
                            (int64_t) speedError * elapsedMs;
        if (candidateIntegral > BALL_SPEED_I_LIMIT_PIXEL_MS) {
            candidateIntegral = BALL_SPEED_I_LIMIT_PIXEL_MS;
        } else if (candidateIntegral < -BALL_SPEED_I_LIMIT_PIXEL_MS) {
            candidateIntegral = -BALL_SPEED_I_LIMIT_PIXEL_MS;
        }
        g_ballSpeedIntegralPixelMs = (int32_t) candidateIntegral;
    }

    if (positionError == 0 &&
        absolute_i32(g_ballMeasuredSpeedPixelsPerS) <=
        (uint32_t) BALL_STOP_SPEED_BAND_PX_S) {
        g_ballSpeedIntegralPixelMs =
            (g_ballSpeedIntegralPixelMs * 3) / 4;
    }

    speedIntegralUMilli = (int32_t) (
        ((int64_t) g_ballSpeedIntegralPixelMs *
         BALL_SPEED_KI_U_PER_PIXEL) / 1000LL);
    tiltCommandUMilli = clamp_i32(
        speedProportionalUMilli + speedIntegralUMilli,
        -BALL_TILT_U_LIMIT_MILLI,
        BALL_TILT_U_LIMIT_MILLI);

    *positionErrorOut = positionError;
    *speedErrorOut = speedError;
    *speedProportionalOut = speedProportionalUMilli;
    *speedIntegralOut = speedIntegralUMilli;
    *tiltCommandOut = tiltCommandUMilli;
    return vision_tilt_to_motor_millideg(tiltCommandUMilli);
}

static void handle_openmv_line(char *line)
{
    uint32_t x;
    int32_t positionError;
    int32_t speedError;
    int32_t speedProportionalUMilli;
    int32_t speedIntegralUMilli;
    int32_t tiltCommandUMilli;
    int32_t targetMillideg;

    if (line[0] == 'N' && line[1] == '\0') {
        reset_ball_controller();
        return; /* No ball: retain and hold the last valid motor target. */
    }
    if (parse_openmv_x(line, &x) == 0U || g_visionEnabled == 0U) {
        return;
    }

    targetMillideg = ball_cascade_calculate(
        x,
        &positionError,
        &speedError,
        &speedProportionalUMilli,
        &speedIntegralUMilli,
        &tiltCommandUMilli);
    update_vision_position_target(targetMillideg);

    g_visionStatusFrameCount++;
    if (g_visionStatusFrameCount >= VISION_STATUS_EVERY_FRAMES) {
        g_visionStatusFrameCount = 0U;
        print_ball_cascade_status(
            x,
            positionError,
            speedError,
            speedProportionalUMilli,
            speedIntegralUMilli,
            tiltCommandUMilli,
            targetMillideg);
    }
}

static void poll_openmv_uart(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(OPENMV_UART_INST)) {
        char character = (char) DL_UART_Main_receiveData(OPENMV_UART_INST);

        if (character == '\r' || character == '\n') {
            if (g_openmvLength != 0U) {
                g_openmvBuffer[g_openmvLength] = '\0';
                handle_openmv_line(g_openmvBuffer);
                g_openmvLength = 0U;
            }
        } else if (g_openmvLength < (OPENMV_BUFFER_SIZE - 1U)) {
            g_openmvBuffer[g_openmvLength] = character;
            g_openmvLength++;
        } else {
            g_openmvLength = 0U;
        }
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

static uint8_t feedback_fault_check(int32_t currentCount)
{
    int32_t movement;

    if (g_feedbackPulseCount < FEEDBACK_CHECK_STEPS) return 0U;

    movement = currentCount - g_feedbackStartCount;
    if (absolute_i32(movement) >= FEEDBACK_CHECK_COUNTS) {
        if ((g_feedbackExpectedDirection > 0 && movement < 0) ||
            (g_feedbackExpectedDirection < 0 && movement > 0)) {
            g_motionState = MOTION_FAULT;
            motor_disable();
            uart_puts("FAULT feedback direction reversed; change DIR_LEVEL_FOR_POSITIVE_ANGLE\r\n");
            print_status();
            return 1U;
        }
        g_feedbackStartCount = currentCount;
        g_feedbackPulseCount = 0U;
    } else if (g_feedbackPulseCount >= NO_FEEDBACK_LIMIT_STEPS) {
        g_motionState = MOTION_FAULT;
        motor_disable();
        uart_puts("FAULT encoder did not move after 64 STEP pulses\r\n");
        print_status();
        return 1U;
    }
    return 0U;
}

static void position_speed_control_process(void)
{
    uint32_t now;
    uint32_t elapsedMs;
    int32_t currentCount;
    int32_t deltaCount;
    int32_t rawSpeed;
    int32_t positionError;
    int32_t speedError;
    int32_t integralDelta;
    int32_t feedForwardStepRate;
    int32_t proportionalCorrection;
    int32_t integralCorrection;
    int32_t stepRate;

    if (g_motionState != MOTION_MOVING &&
        g_motionState != MOTION_HOLDING) return;

    now = g_milliseconds;
    elapsedMs = now - g_lastControlMs;
    if (elapsedMs < CONTROL_PERIOD_MS) return;

    currentCount = encoder_update();
    deltaCount = currentCount - g_lastControlCount;
    rawSpeed = (int32_t) (((int64_t) deltaCount * 1000LL) /
                          (int64_t) elapsedMs);
    g_measuredSpeedCountsPerS +=
        (rawSpeed - g_measuredSpeedCountsPerS) / SPEED_FILTER_DIVISOR;
    g_lastControlCount = currentCount;
    g_lastControlMs = now;

    if (feedback_fault_check(currentCount) != 0U) return;

    positionError = g_targetCount - currentCount;
    if (absolute_i32(positionError) <=
        (uint32_t) POSITION_TOLERANCE_COUNTS) {
        g_targetSpeedCountsPerS = 0;
        g_commandStepRatePps = 0;
        g_speedIntegralError = 0;
        g_stepPhaseAccumulator = 0U;
        if (g_motionState == MOTION_MOVING) {
            g_motionState = MOTION_HOLDING;
            DL_GPIO_clearPins(MOTOR_GPIO_PORT, MOTOR_GPIO_STEP_PIN);
            uart_puts("REACHED and HOLDING ");
            print_status();
        }
        return;
    }

    if (g_motionState == MOTION_HOLDING) {
        g_motionState = MOTION_MOVING;
        g_speedIntegralError = 0;
        g_feedbackStartCount = currentCount;
        g_feedbackPulseCount = 0U;
        g_feedbackExpectedDirection = 0;
    }

    g_targetSpeedCountsPerS = (int32_t) (
        ((int64_t) positionError * POSITION_KP_NUM) / POSITION_KP_DEN);
    g_targetSpeedCountsPerS = clamp_i32(
        g_targetSpeedCountsPerS,
        -MAX_TARGET_SPEED_COUNTS_S,
        MAX_TARGET_SPEED_COUNTS_S);

    speedError = g_targetSpeedCountsPerS - g_measuredSpeedCountsPerS;
    integralDelta = (int32_t) (((int64_t) speedError * elapsedMs) /
                               CONTROL_PERIOD_MS);
    g_speedIntegralError = clamp_i32(
        g_speedIntegralError + integralDelta,
        -SPEED_INTEGRAL_LIMIT,
        SPEED_INTEGRAL_LIMIT);

    feedForwardStepRate = (int32_t) (
        ((int64_t) g_targetSpeedCountsPerS * MOTOR_STEPS_PER_REV) /
        ENCODER_COUNTS_PER_REV);
    proportionalCorrection =
        (speedError * SPEED_KP_NUM) / SPEED_KP_DEN;
    integralCorrection = g_speedIntegralError / SPEED_KI_DIVISOR;
    stepRate = feedForwardStepRate +
               proportionalCorrection + integralCorrection;
    g_commandStepRatePps = clamp_i32(
        stepRate, -MAX_STEP_RATE_PPS, MAX_STEP_RATE_PPS);
}

static void step_scheduler_process(void)
{
    uint32_t now;
    uint32_t elapsedMs;
    uint32_t stepRateMagnitude;
    uint8_t directionLevel;
    int8_t feedbackDirection;

    now = g_milliseconds;
    elapsedMs = now - g_lastStepSchedulerMs;
    if (elapsedMs == 0U) return;
    g_lastStepSchedulerMs = now;

    if (g_motionState != MOTION_MOVING || g_commandStepRatePps == 0) {
        g_stepPhaseAccumulator = 0U;
        return;
    }

    stepRateMagnitude = absolute_i32(g_commandStepRatePps);
    g_stepPhaseAccumulator += stepRateMagnitude * elapsedMs;
    if (g_stepPhaseAccumulator < STEP_SCHEDULER_HZ) return;

    /* Emit at most one pulse per millisecond; discard backlog after UART stalls. */
    g_stepPhaseAccumulator %= STEP_SCHEDULER_HZ;
    feedbackDirection = (g_commandStepRatePps > 0) ? 1 : -1;
    directionLevel = (feedbackDirection > 0) ?
        DIR_LEVEL_FOR_POSITIVE_ANGLE :
        (uint8_t) !DIR_LEVEL_FOR_POSITIVE_ANGLE;

    if (feedbackDirection != g_feedbackExpectedDirection) {
        g_feedbackExpectedDirection = feedbackDirection;
        g_feedbackStartCount = g_lastControlCount;
        g_feedbackPulseCount = 0U;
    }
    motor_emit_step(directionLevel);
    g_feedbackPulseCount++;
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
    g_targetSpeedCountsPerS = 0;
    g_measuredSpeedCountsPerS = 0;
    g_commandStepRatePps = 0;
    g_speedIntegralError = 0;
    g_lastControlCount = 0;
    g_stepPhaseAccumulator = 0U;
    g_feedbackStartCount = 0;
    g_feedbackPulseCount = 0U;
    g_feedbackExpectedDirection = 0;
    g_motionState = MOTION_IDLE;
    g_commandLength = 0U;
    g_openmvLength = 0U;
    g_visionEnabled = 1U;
    g_visionStatusFrameCount = 0U;
    reset_ball_controller();
    g_dirInitialized = 0U;
    g_milliseconds = 0U;
    DL_SYSTICK_config(CPUCLK_FREQ / 1000U);
    g_lastControlMs = g_milliseconds;
    g_lastStepSchedulerMs = g_milliseconds;

    uart_puts("\r\nBall position-speed cascade + motor angle servo ready\r\n");
    uart_puts("Power-on position=0 deg, CCW=positive, CW=negative\r\n");
    uart_puts("Motor actuator: encoder position P + motor speed PI, STEP max=1000 pps.\r\n");
    uart_puts("OpenMV UART1: PA8 TX, PA9 RX; protocol X<pixel> or N\r\n");
    uart_puts("Ball control: position P -> target ball speed; ball speed PI -> tilt u.\r\n");
    uart_puts("Ball equilibrium: x=180, ball speed=0; u=+1000/-1000 -> +30/-40 deg.\r\n");
    uart_puts("Vision motor-angle hard limit: -60..+45 deg.\r\n");
    uart_puts("Power-on vision control ON; no valid ball means no automatic movement.\r\n");
    uart_puts("Commands: V, To30, To-30, To12.5, ?, S (press Enter)\r\n");

    while (1) {
        poll_uart();
        poll_openmv_uart();
        position_speed_control_process();
        step_scheduler_process();
        if (g_motionState != MOTION_MOVING) {
            (void) encoder_update();
        }
    }
}
