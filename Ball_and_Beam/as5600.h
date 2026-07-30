#ifndef AS5600_H
#define AS5600_H

#include <stdbool.h>
#include <stdint.h>

#define AS5600_ADDRESS              (0x36U)
#define AS5600_REG_STATUS           (0x0BU)
#define AS5600_REG_RAW_ANGLE        (0x0CU)

#define AS5600_STATUS_MD            (1U << 5)
#define AS5600_STATUS_ML            (1U << 4)
#define AS5600_STATUS_MH            (1U << 3)

bool as5600_init(void);
bool as5600_read_status(uint8_t *status);
bool as5600_read_raw(uint16_t *raw);
bool as5600_update(float dt);

float as5600_get_angle_rad(void);
float as5600_get_unwrapped_angle_rad(void);
float as5600_get_velocity_rad_s(void);
uint16_t as5600_get_raw(void);
uint8_t as5600_get_status(void);
uint8_t as5600_get_consecutive_failures(void);
bool as5600_magnet_ok(void);
bool as5600_is_healthy(void);

#endif
