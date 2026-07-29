/**
 * @file main.c
 * @brief Static 8-way line sensor -> steering servo test.
 *
 * 74HC4067:
 *   S0 PB15, S1 PB16, S2 PB12, S3 PB13, SIG PB25/ADC0.4
 *   C0..C7 are the eight sensor analog outputs, left to right.
 *
 * Steering servo:
 *   PA22 / TIMG6_CCP1, 50 Hz. Calibrated center = 2850 counts.
 *
 * The rear motor-driver inputs are forced LOW. This firmware never drives
 * either rear motor. UART0 TX is PA10, 115200 8N1.
 */

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_COUNT              8U

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

#define SERVO_TIMER               TIMG6
#define SERVO_PINCM               IOMUX_PINCM47
#define SERVO_PIN_FUNC            IOMUX_PINCM47_PF_TIMG6_CCP1
#define SERVO_CC_INDEX            DL_TIMER_CC_1_INDEX
#define SERVO_CC_REG              DL_TIMERG_CAPTURE_COMPARE_1_INDEX
#define SERVO_PERIOD              40000U
#define SERVO_CENTER              2850U
#define SERVO_TRAVEL              650
#define SERVO_MIN                 (SERVO_CENTER - SERVO_TRAVEL)
#define SERVO_MAX                 (SERVO_CENTER + SERVO_TRAVEL)
#define SERVO_SELF_TEST_OFFSET    120U
/* Change to -1 if a line on the left makes the wheels steer right. */
#define SERVO_DIRECTION          -1

#define DEBUG_UART                UART0
#define DEBUG_TX_PINCM            IOMUX_PINCM21
#define DEBUG_TX_FUNC             IOMUX_PINCM21_PF_UART0_TX

#define CAL_KEY_PORT              GPIOB
#define CAL_KEY_PINCM             IOMUX_PINCM49
#define CAL_KEY_PIN               DL_GPIO_PIN_21

#define MOTOR_A_PORT              GPIOA
#define MOTOR_A_PWM_R_PINCM       IOMUX_PINCM34
#define MOTOR_A_PWM_R_PIN         DL_GPIO_PIN_12
#define MOTOR_A_PWM_L_PINCM       IOMUX_PINCM35
#define MOTOR_A_PWM_L_PIN         DL_GPIO_PIN_13
#define MOTOR_A_DIR_L_PINCM       IOMUX_PINCM38
#define MOTOR_A_DIR_L_PIN         DL_GPIO_PIN_16
#define MOTOR_B_PORT              GPIOB
#define MOTOR_B_DIR_R1_PINCM      IOMUX_PINCM43
#define MOTOR_B_DIR_R1_PIN        DL_GPIO_PIN_17
#define MOTOR_B_DIR_R2_PINCM      IOMUX_PINCM45
#define MOTOR_B_DIR_R2_PIN        DL_GPIO_PIN_19
#define MOTOR_B_DIR_L_PINCM       IOMUX_PINCM52
#define MOTOR_B_DIR_L_PIN         DL_GPIO_PIN_24

static uint16_t g_baseline[SENSOR_COUNT];
static uint16_t g_threshold[SENSOR_COUNT];
static uint16_t g_raw[SENSOR_COUNT];
static uint32_t g_servo_count = SERVO_CENTER;

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

static void motors_force_off(void)
{
    DL_GPIO_initDigitalOutput(MOTOR_A_PWM_R_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_A_PWM_L_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_A_DIR_L_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_B_DIR_R1_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_B_DIR_R2_PINCM);
    DL_GPIO_initDigitalOutput(MOTOR_B_DIR_L_PINCM);

    DL_GPIO_clearPins(MOTOR_A_PORT,
        MOTOR_A_PWM_R_PIN | MOTOR_A_PWM_L_PIN | MOTOR_A_DIR_L_PIN);
    DL_GPIO_clearPins(MOTOR_B_PORT,
        MOTOR_B_DIR_R1_PIN | MOTOR_B_DIR_R2_PIN | MOTOR_B_DIR_L_PIN);
    DL_GPIO_enableOutput(MOTOR_A_PORT,
        MOTOR_A_PWM_R_PIN | MOTOR_A_PWM_L_PIN | MOTOR_A_DIR_L_PIN);
    DL_GPIO_enableOutput(MOTOR_B_PORT,
        MOTOR_B_DIR_R1_PIN | MOTOR_B_DIR_R2_PIN | MOTOR_B_DIR_L_PIN);
}

static void servo_set(uint32_t count)
{
    if (count < SERVO_MIN) {
        count = SERVO_MIN;
    } else if (count > SERVO_MAX) {
        count = SERVO_MAX;
    }
    g_servo_count = count;
    DL_TimerG_setCaptureCompareValue(
        SERVO_TIMER, count, SERVO_CC_INDEX);
}

static void servo_init(void)
{
    DL_TimerG_reset(SERVO_TIMER);
    DL_TimerG_enablePower(SERVO_TIMER);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_Timer_ClockConfig clock = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 15U,
    };
    DL_TimerG_setClockConfig(SERVO_TIMER, &clock);

    DL_Timer_PWMConfig pwm = {
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
        .period = SERVO_PERIOD,
        .isTimerWithFourCC = false,
        .startTimer = DL_TIMER_STOP,
    };
    DL_TimerG_initPWMMode(SERVO_TIMER, &pwm);
    DL_TimerG_setCaptureCompareOutCtl(
        SERVO_TIMER, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL, SERVO_CC_REG);
    DL_TimerG_setCaptCompUpdateMethod(
        SERVO_TIMER, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, SERVO_CC_REG);
    DL_GPIO_initPeripheralOutputFunction(SERVO_PINCM, SERVO_PIN_FUNC);
    DL_TimerG_enableClock(SERVO_TIMER);
    DL_TimerG_setCCPDirection(SERVO_TIMER, DL_TIMER_CC1_OUTPUT);
    servo_set(SERVO_CENTER);
    DL_TimerG_startCounter(SERVO_TIMER);
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

    uart_puts("\r\nCAL: keep all 8 sensors on WHITE, hold still...\r\n");
    servo_set(SERVO_CENTER);
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
        g_threshold[i] = (uint16_t)(noise * 4U + 30U);
        if (g_threshold[i] < 60U) {
            g_threshold[i] = 60U;
        } else if (g_threshold[i] > 400U) {
            g_threshold[i] = 400U;
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
    uart_puts("CAL OK. Slide black line from C0(left) to C7(right).\r\n");
}

static bool calculate_line(int32_t *position, uint32_t *strength_sum)
{
    static const int32_t weight[SENSOR_COUNT] = {
        -3500, -2500, -1500, -500, 500, 1500, 2500, 3500
    };
    int32_t weighted = 0;
    uint32_t total = 0U;
    uint32_t peak_delta = 0U;

    for (uint32_t i = 0U; i < SENSOR_COUNT; i++) {
        uint32_t delta = (g_raw[i] >= g_baseline[i])
            ? (uint32_t)(g_raw[i] - g_baseline[i])
            : (uint32_t)(g_baseline[i] - g_raw[i]);
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
    if ((total < 80U) || (peak_delta < 100U)) {
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
    uart_puts(" servo=");
    uart_u32(g_servo_count);
    uart_puts("\r\n");
}

int main(void)
{
    uint32_t report_divider = 0U;

    SYSCFG_DL_init();
    motors_force_off();
    uart_init();
    servo_init();
    mux_init();
    adc_init();
    DL_GPIO_initDigitalInputFeatures(
        CAL_KEY_PINCM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    uart_puts("\r\n8-way static line -> servo test\r\n");
    uart_puts("C0..C7=left..right, servo PA22 center=2850, motors OFF\r\n");
    uart_puts("Servo self-test: LEFT -> CENTER -> RIGHT -> CENTER\r\n");
    servo_set(SERVO_CENTER - SERVO_SELF_TEST_OFFSET);
    delay_ms(600U);
    servo_set(SERVO_CENTER);
    delay_ms(400U);
    servo_set(SERVO_CENTER + SERVO_SELF_TEST_OFFSET);
    delay_ms(600U);
    servo_set(SERVO_CENTER);
    delay_ms(400U);
    uart_puts("Servo self-test finished; line sensing starts now.\r\n");
    uart_puts("Keep all sensors on WHITE for automatic calibration.\r\n");
    delay_ms(500U);
    calibrate_white();

    while (1) {
        int32_t position;
        uint32_t strength;
        bool found;
        int32_t target;

        sensors_read_all(g_raw);
        found = calculate_line(&position, &strength);
        if (found) {
            target = (int32_t)SERVO_CENTER +
                (SERVO_DIRECTION * position * SERVO_TRAVEL) / 3500;
        } else {
            target = SERVO_CENTER;
        }
        if (target < (int32_t)SERVO_MIN) {
            target = (int32_t)SERVO_MIN;
        } else if (target > (int32_t)SERVO_MAX) {
            target = (int32_t)SERVO_MAX;
        }
        servo_set((3U * g_servo_count + (uint32_t)target) / 4U);

        if (++report_divider >= 5U) {
            report_divider = 0U;
            report(found, position, strength);
        }

        if (key_pressed()) {
            delay_ms(30U);
            if (key_pressed()) {
                calibrate_white();
                while (key_pressed()) {
                    delay_ms(10U);
                }
            }
        }
        delay_ms(20U);
    }
}
