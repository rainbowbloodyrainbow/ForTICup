#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>

typedef enum {
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_SENSOR,
    MOTOR_FAULT_MAGNET,
    MOTOR_FAULT_DRIVER,
    MOTOR_FAULT_CONTROL_OVERRUN,
    MOTOR_FAULT_INVALID_MATH,
    MOTOR_FAULT_PWM,
    MOTOR_FAULT_CALIBRATION
} MotorFault;

typedef struct {
    bool enabled;
    bool calibrated;
    bool sensor_ok;
    bool driver_fault;
    bool fault_latched;
    float target_angle_rad;
    float commanded_angle_rad;
    float measured_angle_rad;
    float measured_velocity_rad_s;
    float velocity_target_rad_s;
    float uq_command_v;
    float electrical_zero_offset;
    int pole_pairs;
    int sensor_direction;
    MotorFault fault;
} MotorControlState;

void motor_control_init(void);
void motor_control_update(float dt);
bool motor_control_calibrate(void);
void motor_control_enable(bool enable);
bool motor_control_try_enable(void);
bool motor_control_take_enable_grace(void);
bool motor_control_set_target_deg(float degrees);
void motor_control_zero_here(void);
void motor_control_reset(void);
bool motor_control_clear_fault(void);
void motor_control_force_fault(MotorFault fault);
const MotorControlState *motor_control_get_state(void);

float motor_control_get_position_kp(void);
float motor_control_get_velocity_kp(void);
float motor_control_get_velocity_ki(void);
float motor_control_get_voltage_limit(void);
float motor_control_get_velocity_limit(void);
bool motor_control_set_position_kp(float value);
bool motor_control_set_velocity_kp(float value);
bool motor_control_set_velocity_ki(float value);
bool motor_control_set_voltage_limit(float value);
bool motor_control_set_velocity_limit(float value);
bool motor_control_set_pole_pairs(int value);
bool motor_control_set_sensor_direction(int value);
const char *motor_control_fault_name(MotorFault fault);

#endif
