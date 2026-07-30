#ifndef FOC_PWM_H
#define FOC_PWM_H

#include <stdbool.h>

void foc_pwm_init(void);
void foc_pwm_enable(bool enable);
void foc_pwm_set_phase_duty(float da, float db, float dc);
void foc_set_phase_voltage(float uq, float electricalAngle);
void foc_pwm_stop(void);
bool foc_pwm_last_write_ok(void);
float normalize_angle_0_2pi(float angle);

#endif
