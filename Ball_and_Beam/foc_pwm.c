#include "foc_pwm.h"

#include <math.h>
#include <stdint.h>

#include "app_config.h"
#include "ti_msp_dl_config.h"

#define DUTY_MIN                    (0.02f)
#define DUTY_MAX                    (0.98f)
#define SQRT3_OVER_2                (0.8660254037844386f)

static bool g_lastWriteOk;

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static uint32_t duty_to_compare(float duty)
{
    /*
     * SysConfig's center-aligned mode loads timerCount / 2 into LOAD.
     * With the generated compare actions, duty = 1 - compare / LOAD.
     */
    uint32_t load = DL_TimerA_getLoadValue(FOC_PWM_INST);
    float compare = (1.0f - duty) * (float) load;

    if (compare < 0.0f) {
        compare = 0.0f;
    } else if (compare > (float) load) {
        compare = (float) load;
    }
    return (uint32_t) (compare + 0.5f);
}

float normalize_angle_0_2pi(float angle)
{
    if (!isfinite(angle)) {
        return 0.0f;
    }
    angle = fmodf(angle, TWO_PI_F);
    if (angle < 0.0f) {
        angle += TWO_PI_F;
    }
    return angle;
}

void foc_pwm_enable(bool enable)
{
    if (enable) {
        DL_GPIO_setPins(FOC_GPIO_PORT, FOC_GPIO_EN_PIN);
    } else {
        DL_GPIO_clearPins(FOC_GPIO_PORT, FOC_GPIO_EN_PIN);
    }
}

void foc_pwm_set_phase_duty(float da, float db, float dc)
{
    uint32_t compareA;
    uint32_t compareB;
    uint32_t compareC;

    if (!isfinite(da) || !isfinite(db) || !isfinite(dc) ||
        (da < 0.0f) || (da > 1.0f) ||
        (db < 0.0f) || (db > 1.0f) ||
        (dc < 0.0f) || (dc > 1.0f)) {
        g_lastWriteOk = false;
        foc_pwm_stop();
        return;
    }

    da = clampf(da, DUTY_MIN, DUTY_MAX);
    db = clampf(db, DUTY_MIN, DUTY_MAX);
    dc = clampf(dc, DUTY_MIN, DUTY_MAX);
    compareA = duty_to_compare(da);
    compareB = duty_to_compare(db);
    compareC = duty_to_compare(dc);

    /*
     * SysConfig selected ZERO_EVT for all three CC shadow registers.
     * These sequential writes therefore become active together on the next
     * timer zero event.
     */
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, compareA, GPIO_FOC_PWM_C0_IDX);
#if MOTOR_PHASE_ORDER == 0
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, compareB, GPIO_FOC_PWM_C1_IDX);
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, compareC, GPIO_FOC_PWM_C3_IDX);
#else
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, compareC, GPIO_FOC_PWM_C1_IDX);
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, compareB, GPIO_FOC_PWM_C3_IDX);
#endif
    g_lastWriteOk = true;
}

void foc_set_phase_voltage(float uq, float electricalAngle)
{
    float uAlpha;
    float uBeta;
    float ua;
    float ub;
    float uc;

    if (!isfinite(uq) || !isfinite(electricalAngle) ||
        !isfinite(BUS_VOLTAGE_V) || (BUS_VOLTAGE_V <= 0.0f)) {
        g_lastWriteOk = false;
        foc_pwm_stop();
        return;
    }

    electricalAngle = normalize_angle_0_2pi(electricalAngle);
    uAlpha = -uq * sinf(electricalAngle);
    uBeta  =  uq * cosf(electricalAngle);
    ua = uAlpha;
    ub = -0.5f * uAlpha + SQRT3_OVER_2 * uBeta;
    uc = -0.5f * uAlpha - SQRT3_OVER_2 * uBeta;

    foc_pwm_set_phase_duty(
        0.5f + ua / BUS_VOLTAGE_V,
        0.5f + ub / BUS_VOLTAGE_V,
        0.5f + uc / BUS_VOLTAGE_V);
}

void foc_pwm_stop(void)
{
    /*
     * Equal 50% duty gives approximately zero line-to-line voltage. Pull EN
     * low as the final hardware-safe state.
     */
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, duty_to_compare(0.5f), GPIO_FOC_PWM_C0_IDX);
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, duty_to_compare(0.5f), GPIO_FOC_PWM_C1_IDX);
    DL_TimerA_setCaptureCompareValue(
        FOC_PWM_INST, duty_to_compare(0.5f), GPIO_FOC_PWM_C3_IDX);
    foc_pwm_enable(false);
}

void foc_pwm_init(void)
{
    g_lastWriteOk = true;
    foc_pwm_enable(false);
    foc_pwm_set_phase_duty(0.5f, 0.5f, 0.5f);
    DL_TimerA_startCounter(FOC_PWM_INST);
}

bool foc_pwm_last_write_ok(void)
{
    return g_lastWriteOk;
}
