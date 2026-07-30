#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * 这些值只是安全的首次上电默认值，必须按实际硬件核实：
 * - MOTOR_POLE_PAIRS：总磁极数 / 2，2204 只是尺寸，不代表极对数。
 * - BUS_VOLTAGE_V：必须与 SimpleFOC Mini 动力电源实测值一致。
 * - 初次测试务必保持很低的 VOLTAGE_LIMIT_V，并使用限流电源。
 */
#define MOTOR_POLE_PAIRS            (7)
#define SENSOR_DIRECTION            (1)
#define MOTOR_PHASE_ORDER           (0)
#define BUS_VOLTAGE_V               (8.0f)
#define VOLTAGE_LIMIT_V             (0.40f)
#define ALIGN_VOLTAGE_V             (0.35f)
#define POSITION_LIMIT_DEG          (20.0f)
#define VELOCITY_LIMIT_RAD_S        (2.0f)
#define CONTROL_FREQUENCY_HZ        (1000.0f)

#define POSITION_KP_DEFAULT         (4.0f)
#define VELOCITY_KP_DEFAULT         (0.20f)
#define VELOCITY_KI_DEFAULT         (0.80f)
#define TARGET_SLEW_RATE_DEG_S      (30.0f)
#define VELOCITY_FILTER_TAU_S       (0.010f)

#define AS5600_I2C_TIMEOUT_LOOPS    (320000U)
#define AS5600_MAX_FAILURES         (3U)
#define CONTROL_OVERRUN_LIMIT       (5U)

#define PI_F                        (3.14159265358979323846f)
#define TWO_PI_F                    (6.28318530717958647692f)
#define DEG_TO_RAD_F                (PI_F / 180.0f)
#define RAD_TO_DEG_F                (180.0f / PI_F)

#endif
