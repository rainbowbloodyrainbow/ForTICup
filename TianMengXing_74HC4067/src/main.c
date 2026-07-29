/**
 * @file main.c
 * @brief Five-way analog line sensor -> three-wheel differential drive test.
 *
 * 74HC4067:
 *   S0 PB15, S1 PB16, S2 PB12, S3 PB13, SIG PB25/ADC0.4
 *   C0..C4 are the five sensor analog outputs, left to right.
 *
 * Differential drive on the TianMengXing expansion board:
 *   Left:  PWM PA13, DIR PA16/PB24, encoder connector PA14/PA25
 *   Right: PWM PA12, DIR PB17/PB19, encoder connector PA26/PA27
 *
 * The front support is a passive caster wheel; there is no steering servo.
 * UART0 TX is PA10, 115200 8N1. Motors stay stopped until PB21 is pressed.
 */

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_COUNT              5U
#define LINE_POSITION_FULL_SCALE  2000
#define LINE_LOST_HOLD_CYCLES     20U
#define LINE_SEARCH_TIMEOUT_CYCLES 200U
#define LINE_CENTER_DEADBAND      80

#define DRIVE_BASE_SPEED_TICKS    45
#define DRIVE_MIN_SPEED_TICKS     18
#define DRIVE_MAX_SPEED_TICKS     55
#define DRIVE_MAX_SPEED_CORRECTION 28
#define DRIVE_SEARCH_INNER_TICKS  18
#define DRIVE_SEARCH_OUTER_TICKS  55
#define DRIVE_MIN_DUTY            140
#define DRIVE_MAX_DUTY            625
#define DRIVE_LINE_KP_DIV         80
#define DRIVE_LINE_KD_DIV         120

/* Per-wheel 10 ms speed PI: PWM = target*FF + KP*error + integral/KI_DIV. */
#define SPEED_FEEDFORWARD         11
#define SPEED_KP                  3
#define SPEED_KI_DIV              8
#define SPEED_INTEGRAL_LIMIT      600
#define SPEED_PI_STEP_TEST_MODE   0
#define SPEED_TEST_STAGE_MS       3000U

#define MUX_PORT                  GPIOB
#define MUX_S0_PINCM              IOMUX_PINCM32
#define MUX_S0_PIN                DL_GPIO_PIN_15
#define MUX_S1_PINCM              IOMUX_PINCM33
#define MUX_S1_PIN                DL_GPIO_PIN_16
#define MUX_S2_PINCM              IOMUX_PINCM29
#define MUX_S2_PIN                DL_GPIO_PIN_12
#define MUX_S3_PINCM              IOMUX_PINCM30
#define MUX_S3_PIN                DL_GPIO_PIN_13
#define MUX_ALL_PINS              (MUX_S0_PIN | MUX_S1_PIN | \
                                   MUX_S2_PIN | MUX_S3_PIN)

#define SENSOR_ADC                ADC0
#define SENSOR_ADC_PINCM          IOMUX_PINCM56
#define SENSOR_ADC_CHANNEL        DL_ADC12_INPUT_CHAN_4

#define DEBUG_UART                UART0
#define DEBUG_TX_PINCM            IOMUX_PINCM21
#define DEBUG_TX_FUNC             IOMUX_PINCM21_PF_UART0_TX

#define CAL_KEY_PORT              GPIOB
#define CAL_KEY_PINCM             IOMUX_PINCM49
#define CAL_KEY_PIN               DL_GPIO_PIN_21

#define MOTOR_TIMER               TIMG0
#define MOTOR_PWM_PERIOD          1600U
#define MOTOR_R_PWM_PINCM         IOMUX_PINCM34
#define MOTOR_R_PWM_FUNC          IOMUX_PINCM34_PF_TIMG0_CCP0
#define MOTOR_R_CC_INDEX          DL_TIMER_CC_0_INDEX
#define MOTOR_R_CC_REG            DL_TIMERG_CAPTURE_COMPARE_0_INDEX
#define MOTOR_L_PWM_PINCM         IOMUX_PINCM35
#define MOTOR_L_PWM_FUNC          IOMUX_PINCM35_PF_TIMG0_CCP1
#define MOTOR_L_CC_INDEX          DL_TIMER_CC_1_INDEX
#define MOTOR_L_CC_REG            DL_TIMERG_CAPTURE_COMPARE_1_INDEX
#define MOTOR_L_IN1_PORT          GPIOA
#define MOTOR_L_IN1_PINCM         IOMUX_PINCM38
#define MOTOR_L_IN1_PIN           DL_GPIO_PIN_16
#define MOTOR_L_IN2_PORT          GPIOB
#define MOTOR_L_IN2_PINCM         IOMUX_PINCM52
#define MOTOR_L_IN2_PIN           DL_GPIO_PIN_24
#define MOTOR_R_DIR_PORT          GPIOB
#define MOTOR_R_IN1_PINCM         IOMUX_PINCM43
#define MOTOR_R_IN1_PIN           DL_GPIO_PIN_17
#define MOTOR_R_IN2_PINCM         IOMUX_PINCM45
#define MOTOR_R_IN2_PIN           DL_GPIO_PIN_19

#define ENC_PORT                  GPIOA
#define ENC_L_A_PINCM             IOMUX_PINCM36
#define ENC_L_A_PIN               DL_GPIO_PIN_14
#define ENC_L_B_PINCM             IOMUX_PINCM55
#define ENC_L_B_PIN               DL_GPIO_PIN_25
#define ENC_R_A_PINCM             IOMUX_PINCM59
#define ENC_R_A_PIN               DL_GPIO_PIN_26
#define ENC_R_B_PINCM             IOMUX_PINCM60
#define ENC_R_B_PIN               DL_GPIO_PIN_27
#define ENC_ALL_PINS              (ENC_L_A_PIN | ENC_L_B_PIN | \
                                   ENC_R_A_PIN | ENC_R_B_PIN)
#define ENC_L_FORWARD_SIGN        (-1)
#define ENC_R_FORWARD_SIGN        (-1)

typedef struct {
    volatile int32_t count;
    volatile uint32_t invalid;
    volatile uint8_t previous;
} EncoderState;

typedef struct {
    int32_t integral;
} SpeedPI;

static uint16_t g_baseline[SENSOR_COUNT];
static uint16_t g_threshold[SENSOR_COUNT];
static uint16_t g_raw[SENSOR_COUNT];
static int32_t g_left_duty;
static int32_t g_right_duty;
static int32_t g_left_speed_ticks;
static int32_t g_right_speed_ticks;
static int32_t g_left_target_ticks;
static int32_t g_right_target_ticks;
static bool g_drive_running;
static bool g_line_searching;
static EncoderState g_left_encoder;
static EncoderState g_right_encoder;
static SpeedPI g_left_speed_pi;
static SpeedPI g_right_speed_pi;
static int32_t g_left_last_count;
static int32_t g_right_last_count;
static uint32_t g_speed_last_ms;
static volatile uint32_t g_millis;

void SysTick_Handler(void)
{
    g_millis++;
}

static void delay_ms(uint32_t ms)
{
    delay_cycles(ms * (CPUCLK_FREQ / 1000U));
}

static void delay_us(uint32_t us)
{
    delay_cycles(us * (CPUCLK_FREQ / 1000000U));
}

static void uart_init(void)
{
    DL_UART_reset(DEBUG_UART);
    DL_UART_enablePower(DEBUG_UART);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_UART_Config cfg = {
        .mode = DL_UART_MODE_NORMAL,
        .direction = DL_UART_DIRECTION_TX,
        .flowControl = DL_UART_FLOW_CONTROL_NONE,
        .parity = DL_UART_PARITY_NONE,
        .wordLength = DL_UART_WORD_LENGTH_8_BITS,
        .stopBits = DL_UART_STOP_BITS_ONE,
    };
    DL_UART_init(DEBUG_UART, &cfg);

    DL_UART_ClockConfig clock = {
        .clockSel = DL_UART_CLOCK_BUSCLK,
        .divideRatio = DL_UART_CLOCK_DIVIDE_RATIO_1,
    };
    DL_UART_setClockConfig(DEBUG_UART, &clock);
    DL_UART_setOversampling(DEBUG_UART, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_setBaudRateDivisor(DEBUG_UART, 17U, 23U);
    DL_UART_enableFIFOs(DEBUG_UART);
    DL_GPIO_initPeripheralOutputFunction(DEBUG_TX_PINCM, DEBUG_TX_FUNC);
    DL_UART_enable(DEBUG_UART);
}

static void uart_putc(char c)
{
    DL_UART_transmitDataBlocking(DEBUG_UART, (uint8_t)c);
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

static void uart_u32(uint32_t value)
{
    char digits[10];
    uint32_t used = 0U;

    do {
        digits[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (used < sizeof(digits)));

    while (used != 0U) {
        uart_putc(digits[--used]);
    }
}

static void uart_i32(int32_t value)
{
    if (value < 0) {
        uart_putc('-');
        uart_u32((uint32_t)(-(value + 1)) + 1U);
    } else {
        uart_u32((uint32_t)value);
    }
}

static uint8_t encoder_read_left(void)
{
    uint32_t pins = DL_GPIO_readPins(
        ENC_PORT, ENC_L_A_PIN | ENC_L_B_PIN);
    return (uint8_t)(((pins & ENC_L_A_PIN) ? 2U : 0U) |
                     ((pins & ENC_L_B_PIN) ? 1U : 0U));
}

static uint8_t encoder_read_right(void)
{
    uint32_t pins = DL_GPIO_readPins(
        ENC_PORT, ENC_R_A_PIN | ENC_R_B_PIN);
    return (uint8_t)(((pins & ENC_R_A_PIN) ? 2U : 0U) |
                     ((pins & ENC_R_B_PIN) ? 1U : 0U));
}

static void encoder_update(EncoderState *encoder, uint8_t current)
{
    static const int8_t transition[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0,
    };
    uint8_t previous = encoder->previous;

    if (current != previous) {
        int8_t step = transition[(uint8_t)((previous << 2) | current)];
        if (step == 0) {
            encoder->invalid++;
        } else {
            encoder->count += step;
        }
        encoder->previous = current;
    }
}

void GROUP1_IRQHandler(void)
{
    uint32_t status = DL_GPIO_getEnabledInterruptStatus(
        ENC_PORT, ENC_ALL_PINS);

    if ((status & (ENC_L_A_PIN | ENC_L_B_PIN)) != 0U) {
        encoder_update(&g_left_encoder, encoder_read_left());
    }
    if ((status & (ENC_R_A_PIN | ENC_R_B_PIN)) != 0U) {
        encoder_update(&g_right_encoder, encoder_read_right());
    }
    DL_GPIO_clearInterruptStatus(ENC_PORT, status);
}

static void encoder_init(void)
{
    DL_GPIO_initDigitalInputFeatures(
        ENC_L_A_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        ENC_L_B_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        ENC_R_A_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        ENC_R_B_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    g_left_encoder.previous = encoder_read_left();
    g_right_encoder.previous = encoder_read_right();
    DL_GPIO_setLowerPinsPolarity(ENC_PORT, DL_GPIO_PIN_14_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(ENC_PORT,
        DL_GPIO_PIN_25_EDGE_RISE_FALL |
        DL_GPIO_PIN_26_EDGE_RISE_FALL |
        DL_GPIO_PIN_27_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(ENC_PORT, ENC_ALL_PINS);
    DL_GPIO_enableInterrupt(ENC_PORT, ENC_ALL_PINS);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
}

static void encoder_sample_speeds(void)
{
    int32_t left_count;
    int32_t right_count;
    uint32_t now_ms;
    uint32_t elapsed_ms;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    left_count = g_left_encoder.count;
    right_count = g_right_encoder.count;
    now_ms = g_millis;
    if (primask == 0U) {
        __enable_irq();
    }

    elapsed_ms = now_ms - g_speed_last_ms;
    if (elapsed_ms == 0U) {
        elapsed_ms = 1U;
    }
    g_left_speed_ticks = (int32_t)(
        ((left_count - g_left_last_count) * ENC_L_FORWARD_SIGN * 10) /
        (int32_t)elapsed_ms);
    g_right_speed_ticks = (int32_t)(
        ((right_count - g_right_last_count) * ENC_R_FORWARD_SIGN * 10) /
        (int32_t)elapsed_ms);
    g_left_last_count = left_count;
    g_right_last_count = right_count;
    g_speed_last_ms = now_ms;
}

static void encoder_reset_speed_window(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    g_left_last_count = g_left_encoder.count;
    g_right_last_count = g_right_encoder.count;
    if (primask == 0U) {
        __enable_irq();
    }
    g_left_speed_ticks = 0;
    g_right_speed_ticks = 0;
    g_speed_last_ms = g_millis;
}

static void motor_set_duty(uint32_t left, uint32_t right)
{
    if (left > MOTOR_PWM_PERIOD) {
        left = MOTOR_PWM_PERIOD;
    }
    if (right > MOTOR_PWM_PERIOD) {
        right = MOTOR_PWM_PERIOD;
    }
    DL_TimerG_setCaptureCompareValue(
        MOTOR_TIMER, left, MOTOR_L_CC_INDEX);
    DL_TimerG_setCaptureCompareValue(
        MOTOR_TIMER, right, MOTOR_R_CC_INDEX);
    g_left_duty = (int32_t)left;
    g_right_duty = (int32_t)right;
}

static void motors_stop(void)
{
    g_left_target_ticks = 0;
    g_right_target_ticks = 0;
    g_left_speed_pi.integral = 0;
    g_right_speed_pi.integral = 0;
    motor_set_duty(0U, 0U);
    DL_GPIO_clearPins(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN);
    DL_GPIO_clearPins(
        MOTOR_R_DIR_PORT, MOTOR_R_IN1_PIN | MOTOR_R_IN2_PIN);
}

static void motors_set_forward(int32_t left, int32_t right)
{
    if (left < DRIVE_MIN_DUTY) {
        left = DRIVE_MIN_DUTY;
    } else if (left > DRIVE_MAX_DUTY) {
        left = DRIVE_MAX_DUTY;
    }
    if (right < DRIVE_MIN_DUTY) {
        right = DRIVE_MIN_DUTY;
    } else if (right > DRIVE_MAX_DUTY) {
        right = DRIVE_MAX_DUTY;
    }

    DL_GPIO_setPins(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN);
    DL_GPIO_setPins(MOTOR_R_DIR_PORT, MOTOR_R_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_R_DIR_PORT, MOTOR_R_IN2_PIN);
    motor_set_duty((uint32_t)left, (uint32_t)right);
}

static int32_t speed_pi_step(
    SpeedPI *controller, int32_t target, int32_t measured)
{
    int32_t error = target - measured;
    int32_t duty;

    controller->integral += error;
    if (controller->integral > SPEED_INTEGRAL_LIMIT) {
        controller->integral = SPEED_INTEGRAL_LIMIT;
    } else if (controller->integral < -SPEED_INTEGRAL_LIMIT) {
        controller->integral = -SPEED_INTEGRAL_LIMIT;
    }

    duty = target * SPEED_FEEDFORWARD +
        error * SPEED_KP +
        controller->integral / SPEED_KI_DIV;
    if (duty < DRIVE_MIN_DUTY) {
        duty = DRIVE_MIN_DUTY;
    } else if (duty > DRIVE_MAX_DUTY) {
        duty = DRIVE_MAX_DUTY;
    }
    return duty;
}

static void motors_set_target_speeds(int32_t left, int32_t right)
{
    int32_t left_duty;
    int32_t right_duty;

    if (left < DRIVE_MIN_SPEED_TICKS) {
        left = DRIVE_MIN_SPEED_TICKS;
    } else if (left > DRIVE_MAX_SPEED_TICKS) {
        left = DRIVE_MAX_SPEED_TICKS;
    }
    if (right < DRIVE_MIN_SPEED_TICKS) {
        right = DRIVE_MIN_SPEED_TICKS;
    } else if (right > DRIVE_MAX_SPEED_TICKS) {
        right = DRIVE_MAX_SPEED_TICKS;
    }

    g_left_target_ticks = left;
    g_right_target_ticks = right;
    left_duty = speed_pi_step(
        &g_left_speed_pi, left, g_left_speed_ticks);
    right_duty = speed_pi_step(
        &g_right_speed_pi, right, g_right_speed_ticks);
    motors_set_forward(left_duty, right_duty);
}

static void motors_init(void)
{
    DL_GPIO_initDigitalOutput(MOTOR_L_IN1_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_L_IN2_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_R_IN1_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_R_IN2_PINCM);
    DL_GPIO_enableOutput(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN);
    DL_GPIO_enableOutput(MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN);
    DL_GPIO_enableOutput(
        MOTOR_R_DIR_PORT, MOTOR_R_IN1_PIN | MOTOR_R_IN2_PIN);

    DL_TimerG_reset(MOTOR_TIMER);
    DL_TimerG_enablePower(MOTOR_TIMER);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_Timer_ClockConfig clock = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0U,
    };
    DL_TimerG_setClockConfig(MOTOR_TIMER, &clock);

    DL_Timer_PWMConfig pwm = {
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
        .period = MOTOR_PWM_PERIOD,
        .isTimerWithFourCC = false,
        .startTimer = DL_TIMER_STOP,
    };
    DL_TimerG_initPWMMode(MOTOR_TIMER, &pwm);
    DL_TimerG_setCaptureCompareOutCtl(
        MOTOR_TIMER, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL, MOTOR_R_CC_REG);
    DL_TimerG_setCaptureCompareOutCtl(
        MOTOR_TIMER, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL, MOTOR_L_CC_REG);
    DL_TimerG_setCaptCompUpdateMethod(
        MOTOR_TIMER, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, MOTOR_R_CC_REG);
    DL_TimerG_setCaptCompUpdateMethod(
        MOTOR_TIMER, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, MOTOR_L_CC_REG);
    DL_GPIO_initPeripheralOutputFunction(
        MOTOR_R_PWM_PINCM, MOTOR_R_PWM_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        MOTOR_L_PWM_PINCM, MOTOR_L_PWM_FUNC);
    DL_TimerG_enableClock(MOTOR_TIMER);
    DL_TimerG_setCCPDirection(
        MOTOR_TIMER, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT);
    DL_TimerG_startCounter(MOTOR_TIMER);
    motors_stop();
}

static void mux_init(void)
{
    DL_GPIO_initDigitalOutput(MUX_S0_PINCM);
    DL_GPIO_initDigitalOutput(MUX_S1_PINCM);
    DL_GPIO_initDigitalOutput(MUX_S2_PINCM);
    DL_GPIO_initDigitalOutput(MUX_S3_PINCM);
    DL_GPIO_clearPins(MUX_PORT, MUX_ALL_PINS);
    DL_GPIO_enableOutput(MUX_PORT, MUX_ALL_PINS);
}

static void mux_select(uint32_t channel)
{
    DL_GPIO_clearPins(MUX_PORT, MUX_ALL_PINS);
    if ((channel & 1U) != 0U) {
        DL_GPIO_setPins(MUX_PORT, MUX_S0_PIN);
    }
    if ((channel & 2U) != 0U) {
        DL_GPIO_setPins(MUX_PORT, MUX_S1_PIN);
    }
    if ((channel & 4U) != 0U) {
        DL_GPIO_setPins(MUX_PORT, MUX_S2_PIN);
    }
    if ((channel & 8U) != 0U) {
        DL_GPIO_setPins(MUX_PORT, MUX_S3_PIN);
    }
}

static void adc_init(void)
{
    DL_ADC12_reset(SENSOR_ADC);
    DL_ADC12_enablePower(SENSOR_ADC);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_ADC12_ClockConfig clock = {
        .clockSel = DL_ADC12_CLOCK_SYSOSC,
        .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
        .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
    };
    DL_ADC12_setClockConfig(SENSOR_ADC, &clock);
    DL_ADC12_initSingleSample(SENSOR_ADC,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(SENSOR_ADC, DL_ADC12_MEM_IDX_0,
        SENSOR_ADC_CHANNEL, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(
        SENSOR_ADC, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(SENSOR_ADC, 160U);
    DL_GPIO_initPeripheralAnalogFunction(SENSOR_ADC_PINCM);
    DL_ADC12_enableConversions(SENSOR_ADC);
}

static uint16_t adc_once(void)
{
    uint16_t result;

    DL_ADC12_startConversion(SENSOR_ADC);
    while ((DL_ADC12_getStatus(SENSOR_ADC) &
            DL_ADC12_STATUS_CONVERSION_ACTIVE) != 0U) {
    }
    result = (uint16_t)DL_ADC12_getMemResult(
        SENSOR_ADC, DL_ADC12_MEM_IDX_0);
    /* A non-repeating single conversion disables further conversions. */
    DL_ADC12_enableConversions(SENSOR_ADC);
    return result;
}

static uint16_t sensor_read_one(uint32_t channel)
{
    uint32_t sum = 0U;

    mux_select(channel);
    delay_us(20U);
    (void)adc_once(); /* Discard after switching the analog multiplexer. */
    for (uint32_t i = 0U; i < 4U; i++) {
        sum += adc_once();
    }
    return (uint16_t)(sum / 4U);
}

static void sensors_read_all(uint16_t *values)
{
    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        values[i] = sensor_read_one(i);
    }
}

static bool key_pressed(void)
{
    return (DL_GPIO_readPins(CAL_KEY_PORT, CAL_KEY_PIN) &
            CAL_KEY_PIN) == 0U;
}

static void calibrate_white(void)
{
    uint32_t sum[SENSOR_COUNT] = {0U};
    uint16_t low[SENSOR_COUNT];
    uint16_t high[SENSOR_COUNT];

    uart_puts("\r\nCAL: keep all 5 sensors on WHITE, hold still...\r\n");
    sensors_read_all(g_raw);
    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        low[i] = g_raw[i];
        high[i] = g_raw[i];
    }

    for (uint32_t sample = 0U; sample < 128U; sample++) {
        sensors_read_all(g_raw);
        for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
            uint16_t value = g_raw[i];
            sum[i] += value;
            if (value < low[i]) {
                low[i] = value;
            }
            if (value > high[i]) {
                high[i] = value;
            }
        }
        delay_ms(5U);
    }

    uart_puts("BASE=");
    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        uint32_t noise;
        g_baseline[i] = (uint16_t)(sum[i] / 128U);
        noise = (uint32_t)(high[i] - low[i]);
        /*
         * The measured module polarity is: black line -> ADC rises.
         * Keep the threshold close to the measured white-floor noise so the
         * weak contrast seen at a 10-20 mm mounting height is still usable.
         */
        g_threshold[i] = (uint16_t)(noise + 8U);
        if (g_threshold[i] < 12U) {
            g_threshold[i] = 12U;
        } else if (g_threshold[i] > 80U) {
            g_threshold[i] = 80U;
        }
        uart_u32(g_baseline[i]);
        uart_putc((i == SENSOR_COUNT - 1U) ? ' ' : ',');
    }
    uart_puts(" TH=");
    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        uart_u32(g_threshold[i]);
        uart_putc((i == SENSOR_COUNT - 1U) ? '\r' : ',');
    }
    uart_putc('\n');
    uart_puts("CAL OK. Slide black line from C0(left) to C4(right).\r\n");
}

static bool calculate_line(int32_t *position, uint32_t *strength_sum)
{
    static const int32_t weight[SENSOR_COUNT] = {
        -2000, -1000, 0, 1000, 2000
    };
    int32_t weighted = 0;
    uint32_t total = 0U;
    uint32_t peak_delta = 0U;

    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        uint32_t delta = (g_raw[i] > g_baseline[i])
            ? (uint32_t)(g_raw[i] - g_baseline[i])
            : 0U;
        uint32_t strength = 0U;

        if (delta > peak_delta) {
            peak_delta = delta;
        }
        if (delta > g_threshold[i]) {
            strength = delta - g_threshold[i];
            total += strength;
            weighted += weight[i] * (int32_t)strength;
        }
    }

    *strength_sum = total;
    if ((total < 15U) || (peak_delta < 15U)) {
        *position = 0;
        return false;
    }
    *position = weighted / (int32_t)total;
    return true;
}

static void report(bool found, int32_t position, uint32_t strength)
{
    uart_puts("ADC[");
    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        uart_u32(g_raw[i]);
        uart_putc((i == SENSOR_COUNT - 1U) ? ']' : ',');
    }
    uart_puts(found ? " LINE " : " LOST ");
    uart_puts("pos=");
    uart_i32(position);
    uart_puts(" sum=");
    uart_u32(strength);
    if (!g_drive_running) {
        uart_puts(" STOP");
    } else if (g_line_searching) {
        uart_puts(" SEARCH");
    } else {
        uart_puts(" RUN");
    }
    uart_puts(" motor[L=");
    uart_i32(g_left_duty);
    uart_puts(" R=");
    uart_i32(g_right_duty);
    uart_puts("] speed[L=");
    uart_i32(g_left_speed_ticks);
    uart_putc('/');
    uart_i32(g_left_target_ticks);
    uart_puts(" R=");
    uart_i32(g_right_speed_ticks);
    uart_putc('/');
    uart_i32(g_right_target_ticks);
    uart_putc(']');
    uart_puts("\r\n");
}

#if SPEED_PI_STEP_TEST_MODE
static void speed_pi_step_test_loop(void)
{
    static const int32_t target_sequence[] = {25, 35, 45, 55};
    const uint32_t stage_count =
        (uint32_t)(sizeof(target_sequence) / sizeof(target_sequence[0]));
    uint32_t test_start_ms = 0U;
    uint32_t report_divider = 0U;

    uart_puts("\r\nDual encoder PI step test (line sensor bypassed)\r\n");
    uart_puts("Press B21: 25 -> 35 -> 45 -> 55 ticks/10ms, 3s each.\r\n");
    motors_stop();

    while (1) {
        int32_t target = 0;
        uint32_t stage = 0U;

        encoder_sample_speeds();
        if (g_drive_running) {
            stage = (g_millis - test_start_ms) / SPEED_TEST_STAGE_MS;
            if (stage >= stage_count) {
                g_drive_running = false;
                motors_stop();
                uart_puts("STEP TEST COMPLETE\r\n");
            } else {
                target = target_sequence[stage];
                motors_set_target_speeds(target, target);
            }
        } else {
            motors_stop();
        }

        if (++report_divider >= 10U) {
            report_divider = 0U;
            uart_puts(g_drive_running ? "TEST target=" : "TEST STOP target=");
            uart_i32(target);
            uart_puts(" speed[L=");
            uart_i32(g_left_speed_ticks);
            uart_puts(" R=");
            uart_i32(g_right_speed_ticks);
            uart_puts("] pwm[L=");
            uart_i32(g_left_duty);
            uart_puts(" R=");
            uart_i32(g_right_duty);
            uart_puts("]\r\n");
        }

        if (key_pressed()) {
            delay_ms(30U);
            if (key_pressed()) {
                bool was_running = g_drive_running;
                while (key_pressed()) {
                    delay_ms(10U);
                }
                if (was_running) {
                    g_drive_running = false;
                    motors_stop();
                    uart_puts("STEP TEST ABORTED\r\n");
                } else {
                    motors_stop();
                    encoder_reset_speed_window();
                    test_start_ms = g_millis;
                    g_drive_running = true;
                    uart_puts("STEP TEST START\r\n");
                }
            }
        }
        delay_ms(10U);
    }
}
#endif

int main(void)
{
    uint32_t report_divider = 0U;

    SYSCFG_DL_init();
    (void)SysTick_Config(CPUCLK_FREQ / 1000U);
    uart_init();
    motors_init();
    encoder_init();
    DL_GPIO_initDigitalInputFeatures(
        CAL_KEY_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

#if SPEED_PI_STEP_TEST_MODE
    speed_pi_step_test_loop();
#endif

    mux_init();
    adc_init();
    uart_puts("\r\n5-way line PD + dual encoder speed PI\r\n");
    uart_puts("C0..C4=left..right, LEFT encoder=A14/A25, RIGHT=A26/A27\r\n");
    uart_puts("Motors stay STOPPED after calibration. Press B21 to RUN/STOP.\r\n");
    uart_puts("Keep all sensors on WHITE for automatic calibration.\r\n");
    delay_ms(500U);
    calibrate_white();
    encoder_reset_speed_window();

    while (1) {
        static int32_t tracked_position = 0;
        static int32_t previous_position = 0;
        static int32_t last_search_direction = 0;
        static uint32_t lost_cycles = LINE_LOST_HOLD_CYCLES + 1U;
        int32_t position;
        uint32_t strength;
        bool found;
        int32_t correction;
        int32_t left_target;
        int32_t right_target;

        encoder_sample_speeds();
        sensors_read_all(g_raw);
        found = calculate_line(&position, &strength);
        if (found) {
            if (lost_cycles > LINE_LOST_HOLD_CYCLES) {
                tracked_position = position;
            } else {
                tracked_position = (tracked_position + position) / 2;
            }
            if (tracked_position < -LINE_CENTER_DEADBAND) {
                last_search_direction = -1;
            } else if (tracked_position > LINE_CENTER_DEADBAND) {
                last_search_direction = 1;
            }
            lost_cycles = 0U;
        } else {
            if (lost_cycles <= LINE_SEARCH_TIMEOUT_CYCLES) {
                lost_cycles++;
            }
        }
        if ((tracked_position > -LINE_CENTER_DEADBAND) &&
            (tracked_position < LINE_CENTER_DEADBAND)) {
            tracked_position = 0;
        }

        correction = tracked_position / DRIVE_LINE_KP_DIV +
            (tracked_position - previous_position) / DRIVE_LINE_KD_DIV;
        previous_position = tracked_position;
        if (correction > DRIVE_MAX_SPEED_CORRECTION) {
            correction = DRIVE_MAX_SPEED_CORRECTION;
        } else if (correction < -DRIVE_MAX_SPEED_CORRECTION) {
            correction = -DRIVE_MAX_SPEED_CORRECTION;
        }

        /*
         * Negative position means the line is left: slow the left wheel and
         * speed up the right wheel. Both wheels remain forward-only.
         */
        left_target = DRIVE_BASE_SPEED_TICKS + correction;
        right_target = DRIVE_BASE_SPEED_TICKS - correction;
        g_line_searching = false;
        if (g_drive_running) {
            if (found || (lost_cycles <= LINE_LOST_HOLD_CYCLES)) {
                motors_set_target_speeds(left_target, right_target);
            } else if ((lost_cycles <= LINE_SEARCH_TIMEOUT_CYCLES) &&
                       (last_search_direction != 0)) {
                g_line_searching = true;
                if (last_search_direction < 0) {
                    /* Last line position was left: arc left to reacquire it. */
                    motors_set_target_speeds(
                        DRIVE_SEARCH_INNER_TICKS, DRIVE_SEARCH_OUTER_TICKS);
                } else {
                    /* Last line position was right: arc right to reacquire it. */
                    motors_set_target_speeds(
                        DRIVE_SEARCH_OUTER_TICKS, DRIVE_SEARCH_INNER_TICKS);
                }
            } else {
                motors_stop();
            }
        } else {
            motors_stop();
        }

        if (++report_divider >= 10U) {
            report_divider = 0U;
            report(found, position, strength);
        }

        if (key_pressed()) {
            delay_ms(30U);
            if (key_pressed()) {
                g_drive_running = !g_drive_running;
                if (!g_drive_running) {
                    g_line_searching = false;
                    motors_stop();
                    uart_puts("DRIVE STOP\r\n");
                } else {
                    previous_position = tracked_position;
                    encoder_reset_speed_window();
                    uart_puts("DRIVE RUN\r\n");
                }
                while (key_pressed()) {
                    delay_ms(10U);
                }
            }
        }
        delay_ms(10U);
    }
}
