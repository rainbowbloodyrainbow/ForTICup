#include "motor_control.h"

#include <math.h>
#include <stdint.h>

#include "app_config.h"
#include "as5600.h"
#include "foc_pwm.h"
#include "ti_msp_dl_config.h"

#define ALIGN_TIME_MS               (700U)
#define POSITION_KP_MAX             (100.0f)
#define VELOCITY_KP_MAX             (20.0f)
#define VELOCITY_KI_MAX             (100.0f)
#define VELOCITY_LIMIT_MAX          (20.0f)
#define POLE_PAIRS_MAX              (30)
#define MIN_VOLTAGE_LIMIT           (0.05f)
#define MAX_VOLTAGE_LIMIT           (BUS_VOLTAGE_V * 0.50f)

static MotorControlState g_motor;
static float g_zeroPositionRad;
static float g_positionKp;
static float g_velocityKp;
static float g_velocityKi;
static float g_voltageLimit;
static float g_velocityLimit;
static float g_velocityIntegral;
static bool g_enableGrace;

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

static bool driver_fault_active(void)
{
    return ((DL_GPIO_readPins(FOC_GPIO_PORT, FOC_GPIO_NFAULT_PIN) &
             FOC_GPIO_NFAULT_PIN) == 0U);
}

static void reset_controller(void)
{
    g_velocityIntegral = 0.0f;
    g_motor.velocity_target_rad_s = 0.0f;
    g_motor.uq_command_v = 0.0f;
}

void motor_control_force_fault(MotorFault fault)
{
    foc_pwm_stop();
    reset_controller();
    g_motor.enabled = false;
    g_motor.fault_latched = true;
    if (g_motor.fault == MOTOR_FAULT_NONE) {
        g_motor.fault = fault;
    }
}

void motor_control_init(void)
{
    g_motor = (MotorControlState) {0};
    g_motor.pole_pairs = MOTOR_POLE_PAIRS;
    g_motor.sensor_direction = SENSOR_DIRECTION;
    g_motor.sensor_ok = as5600_is_healthy();
    g_motor.driver_fault = driver_fault_active();
    g_positionKp = POSITION_KP_DEFAULT;
    g_velocityKp = VELOCITY_KP_DEFAULT;
    g_velocityKi = VELOCITY_KI_DEFAULT;
    g_voltageLimit = VOLTAGE_LIMIT_V;
    g_velocityLimit = VELOCITY_LIMIT_RAD_S;
    g_enableGrace = false;
    g_zeroPositionRad =
        (float) g_motor.sensor_direction *
        as5600_get_unwrapped_angle_rad();
    reset_controller();
    foc_pwm_stop();
}

void motor_control_enable(bool enable)
{
    if (!enable) {
        foc_pwm_stop();
        reset_controller();
        g_motor.enabled = false;
        return;
    }
    (void) motor_control_try_enable();
}

bool motor_control_try_enable(void)
{
    g_motor.sensor_ok = as5600_is_healthy();
    g_motor.driver_fault = driver_fault_active();
    if (!g_motor.calibrated || !g_motor.sensor_ok ||
        g_motor.driver_fault || g_motor.fault_latched) {
        foc_pwm_stop();
        g_motor.enabled = false;
        return false;
    }

    reset_controller();
    g_motor.commanded_angle_rad = g_motor.measured_angle_rad;
    foc_pwm_enable(true);
    g_motor.enabled = true;
    g_enableGrace = true;
    return true;
}

bool motor_control_take_enable_grace(void)
{
    bool grace = g_enableGrace;
    g_enableGrace = false;
    return grace;
}

bool motor_control_calibrate(void)
{
    uint32_t elapsedMs;
    float mechanicalRaw;

    motor_control_enable(false);
    g_motor.sensor_ok = as5600_is_healthy();
    g_motor.driver_fault = driver_fault_active();
    if (!g_motor.sensor_ok || !as5600_magnet_ok() ||
        g_motor.driver_fault || g_motor.fault_latched) {
        motor_control_force_fault(MOTOR_FAULT_CALIBRATION);
        return false;
    }

    g_motor.target_angle_rad = 0.0f;
    g_motor.commanded_angle_rad = 0.0f;
    reset_controller();
    foc_pwm_enable(true);

    /*
     * foc_set_phase_voltage(Uq, 3*pi/2) creates a fixed +alpha stator
     * voltage vector with this project's u_alpha/u_beta sign convention.
     * After the rotor settles there, its electrical angle is defined as 0.
     */
    foc_set_phase_voltage(ALIGN_VOLTAGE_V, 1.5f * PI_F);
    if (!foc_pwm_last_write_ok()) {
        motor_control_force_fault(MOTOR_FAULT_PWM);
        return false;
    }

    for (elapsedMs = 0U; elapsedMs < ALIGN_TIME_MS; elapsedMs++) {
        delay_cycles(CPUCLK_FREQ / 1000U);
        if ((elapsedMs % 10U) == 0U) {
            if (!as5600_update(0.010f) || driver_fault_active()) {
                motor_control_force_fault(MOTOR_FAULT_CALIBRATION);
                return false;
            }
        }
    }

    if (!as5600_update(0.001f) || !as5600_magnet_ok() ||
        driver_fault_active()) {
        motor_control_force_fault(MOTOR_FAULT_CALIBRATION);
        return false;
    }

    mechanicalRaw = as5600_get_unwrapped_angle_rad();
    g_motor.electrical_zero_offset = normalize_angle_0_2pi(
        -(float) g_motor.pole_pairs *
        (float) g_motor.sensor_direction * mechanicalRaw);
    g_zeroPositionRad =
        (float) g_motor.sensor_direction * mechanicalRaw;
    g_motor.measured_angle_rad = 0.0f;
    g_motor.measured_velocity_rad_s = 0.0f;
    g_motor.target_angle_rad = 0.0f;
    g_motor.commanded_angle_rad = 0.0f;
    g_motor.calibrated = true;

    foc_set_phase_voltage(0.0f, 0.0f);
    foc_pwm_enable(false);
    return true;
}

void motor_control_update(float dt)
{
    float sensorPosition;
    float maxCommandStep;
    float positionError;
    float velocityError;
    float integralCandidate;
    float unsaturatedUq;
    float electricalAngle;

    g_motor.sensor_ok = as5600_is_healthy();
    g_motor.driver_fault = driver_fault_active();

    if (!isfinite(dt) || (dt <= 0.0f)) {
        motor_control_force_fault(MOTOR_FAULT_INVALID_MATH);
        return;
    }
    if (!g_motor.sensor_ok) {
        motor_control_force_fault(as5600_magnet_ok() ?
            MOTOR_FAULT_SENSOR : MOTOR_FAULT_MAGNET);
        return;
    }
    if (g_motor.driver_fault) {
        motor_control_force_fault(MOTOR_FAULT_DRIVER);
        return;
    }

    sensorPosition =
        (float) g_motor.sensor_direction *
        as5600_get_unwrapped_angle_rad();
    g_motor.measured_angle_rad = sensorPosition - g_zeroPositionRad;
    g_motor.measured_velocity_rad_s =
        (float) g_motor.sensor_direction *
        as5600_get_velocity_rad_s();

    if (!isfinite(g_motor.measured_angle_rad) ||
        !isfinite(g_motor.measured_velocity_rad_s)) {
        motor_control_force_fault(MOTOR_FAULT_INVALID_MATH);
        return;
    }

    if (!g_motor.enabled || !g_motor.calibrated ||
        g_motor.fault_latched) {
        g_motor.uq_command_v = 0.0f;
        reset_controller();
        foc_pwm_stop();
        return;
    }

    maxCommandStep = TARGET_SLEW_RATE_DEG_S * DEG_TO_RAD_F * dt;
    g_motor.commanded_angle_rad += clampf(
        g_motor.target_angle_rad - g_motor.commanded_angle_rad,
        -maxCommandStep, maxCommandStep);

    positionError =
        g_motor.commanded_angle_rad - g_motor.measured_angle_rad;
    g_motor.velocity_target_rad_s = clampf(
        g_positionKp * positionError, -g_velocityLimit, g_velocityLimit);
    velocityError =
        g_motor.velocity_target_rad_s - g_motor.measured_velocity_rad_s;

    integralCandidate = g_velocityIntegral + velocityError * dt;
    if (g_velocityKi > 0.0f) {
        float integralLimit = g_voltageLimit / g_velocityKi;
        integralCandidate =
            clampf(integralCandidate, -integralLimit, integralLimit);
    } else {
        integralCandidate = 0.0f;
    }

    unsaturatedUq =
        g_velocityKp * velocityError + g_velocityKi * integralCandidate;
    g_motor.uq_command_v =
        clampf(unsaturatedUq, -g_voltageLimit, g_voltageLimit);

    /* Conditional integration prevents winding up farther into saturation. */
    if ((unsaturatedUq == g_motor.uq_command_v) ||
        ((unsaturatedUq > g_voltageLimit) && (velocityError < 0.0f)) ||
        ((unsaturatedUq < -g_voltageLimit) && (velocityError > 0.0f))) {
        g_velocityIntegral = integralCandidate;
    }

    electricalAngle = normalize_angle_0_2pi(
        (float) g_motor.pole_pairs *
        (float) g_motor.sensor_direction *
        as5600_get_unwrapped_angle_rad() +
        g_motor.electrical_zero_offset);
    foc_set_phase_voltage(g_motor.uq_command_v, electricalAngle);
    if (!foc_pwm_last_write_ok()) {
        motor_control_force_fault(MOTOR_FAULT_PWM);
    }
}

bool motor_control_set_target_deg(float degrees)
{
    if (!isfinite(degrees) ||
        (degrees < -POSITION_LIMIT_DEG) ||
        (degrees > POSITION_LIMIT_DEG)) {
        return false;
    }
    g_motor.target_angle_rad = degrees * DEG_TO_RAD_F;
    return true;
}

void motor_control_zero_here(void)
{
    g_zeroPositionRad =
        (float) g_motor.sensor_direction *
        as5600_get_unwrapped_angle_rad();
    g_motor.target_angle_rad = 0.0f;
    g_motor.commanded_angle_rad = 0.0f;
    g_motor.measured_angle_rad = 0.0f;
    reset_controller();
}

void motor_control_reset(void)
{
    motor_control_enable(false);
    g_motor.target_angle_rad = 0.0f;
    g_motor.commanded_angle_rad = 0.0f;
    reset_controller();
}

bool motor_control_clear_fault(void)
{
    if (g_motor.enabled || driver_fault_active() ||
        !as5600_is_healthy() || !as5600_magnet_ok()) {
        return false;
    }
    g_motor.driver_fault = false;
    g_motor.sensor_ok = true;
    g_motor.fault_latched = false;
    g_motor.fault = MOTOR_FAULT_NONE;
    reset_controller();
    foc_pwm_stop();
    return true;
}

const MotorControlState *motor_control_get_state(void)
{
    return &g_motor;
}

float motor_control_get_position_kp(void) { return g_positionKp; }
float motor_control_get_velocity_kp(void) { return g_velocityKp; }
float motor_control_get_velocity_ki(void) { return g_velocityKi; }
float motor_control_get_voltage_limit(void) { return g_voltageLimit; }
float motor_control_get_velocity_limit(void) { return g_velocityLimit; }

bool motor_control_set_position_kp(float value)
{
    if (!isfinite(value) || (value < 0.0f) || (value > POSITION_KP_MAX)) {
        return false;
    }
    g_positionKp = value;
    return true;
}

bool motor_control_set_velocity_kp(float value)
{
    if (!isfinite(value) || (value < 0.0f) || (value > VELOCITY_KP_MAX)) {
        return false;
    }
    g_velocityKp = value;
    return true;
}

bool motor_control_set_velocity_ki(float value)
{
    if (!isfinite(value) || (value < 0.0f) || (value > VELOCITY_KI_MAX)) {
        return false;
    }
    g_velocityKi = value;
    g_velocityIntegral = 0.0f;
    return true;
}

bool motor_control_set_voltage_limit(float value)
{
    if (!isfinite(value) || (value < MIN_VOLTAGE_LIMIT) ||
        (value > MAX_VOLTAGE_LIMIT)) {
        return false;
    }
    g_voltageLimit = value;
    g_velocityIntegral = 0.0f;
    return true;
}

bool motor_control_set_velocity_limit(float value)
{
    if (!isfinite(value) || (value <= 0.0f) ||
        (value > VELOCITY_LIMIT_MAX)) {
        return false;
    }
    g_velocityLimit = value;
    return true;
}

bool motor_control_set_pole_pairs(int value)
{
    if ((value < 1) || (value > POLE_PAIRS_MAX) || g_motor.enabled) {
        return false;
    }
    g_motor.pole_pairs = value;
    g_motor.calibrated = false;
    return true;
}

bool motor_control_set_sensor_direction(int value)
{
    if (((value != 1) && (value != -1)) || g_motor.enabled) {
        return false;
    }
    g_motor.sensor_direction = value;
    g_motor.calibrated = false;
    g_zeroPositionRad =
        (float) value * as5600_get_unwrapped_angle_rad();
    return true;
}

const char *motor_control_fault_name(MotorFault fault)
{
    switch (fault) {
    case MOTOR_FAULT_NONE:            return "none";
    case MOTOR_FAULT_SENSOR:          return "sensor";
    case MOTOR_FAULT_MAGNET:          return "magnet";
    case MOTOR_FAULT_DRIVER:          return "driver";
    case MOTOR_FAULT_CONTROL_OVERRUN: return "overrun";
    case MOTOR_FAULT_INVALID_MATH:    return "math";
    case MOTOR_FAULT_PWM:             return "pwm";
    case MOTOR_FAULT_CALIBRATION:     return "calibration";
    default:                          return "unknown";
    }
}
