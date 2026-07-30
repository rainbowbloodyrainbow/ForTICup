#include "as5600.h"

#include <math.h>
#include <stddef.h>

#include "app_config.h"
#include "ti_msp_dl_config.h"

typedef struct {
    uint16_t raw;
    uint16_t lastRaw;
    uint8_t status;
    uint8_t failures;
    uint16_t statusDivider;
    bool initialized;
    bool healthy;
    float angleRad;
    float unwrappedRad;
    float velocityRadS;
} AS5600State;

static AS5600State g_as5600;

static void i2c_recover(void)
{
    DL_I2C_resetControllerTransfer(AS5600_I2C_INST);
    DL_I2C_flushControllerTXFIFO(AS5600_I2C_INST);
    DL_I2C_flushControllerRXFIFO(AS5600_I2C_INST);
}

static bool i2c_wait_idle(void)
{
    uint32_t timeout = AS5600_I2C_TIMEOUT_LOOPS;

    while (timeout-- > 0U) {
        uint32_t status = DL_I2C_getControllerStatus(AS5600_I2C_INST);
        if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
            return false;
        }
        if ((status & DL_I2C_CONTROLLER_STATUS_IDLE) != 0U) {
            return true;
        }
    }
    return false;
}

static bool i2c_wait_transfer_done(void)
{
    uint32_t timeout = AS5600_I2C_TIMEOUT_LOOPS;

    /*
     * I2C_ERR_13 workaround: starting a transfer must be followed by at
     * least three functional-clock cycles before polling BUSY.
     */
    delay_cycles(100U);
    while (timeout-- > 0U) {
        uint32_t status = DL_I2C_getControllerStatus(AS5600_I2C_INST);
        if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
            return false;
        }
        if ((status & DL_I2C_CONTROLLER_STATUS_BUSY) == 0U) {
            return true;
        }
    }
    return false;
}

static bool read_registers(uint8_t reg, uint8_t *data, uint16_t length)
{
    uint16_t received = 0U;
    uint32_t timeout;

    if ((data == NULL) || (length == 0U)) {
        return false;
    }
    if (!i2c_wait_idle()) {
        i2c_recover();
        return false;
    }

    i2c_recover();
    if (DL_I2C_fillControllerTXFIFO(AS5600_I2C_INST, &reg, 1U) != 1U) {
        i2c_recover();
        return false;
    }
    /*
     * AS5600_ADDRESS is the unshifted 7-bit address 0x36. DriverLib performs
     * the address-field shift internally.
     */
    DL_I2C_startControllerTransfer(AS5600_I2C_INST, AS5600_ADDRESS,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U);
    if (!i2c_wait_transfer_done() || !i2c_wait_idle()) {
        i2c_recover();
        return false;
    }

    DL_I2C_flushControllerRXFIFO(AS5600_I2C_INST);
    DL_I2C_startControllerTransfer(AS5600_I2C_INST, AS5600_ADDRESS,
        DL_I2C_CONTROLLER_DIRECTION_RX, length);

    timeout = AS5600_I2C_TIMEOUT_LOOPS;
    delay_cycles(100U);
    while ((received < length) && (timeout-- > 0U)) {
        uint32_t status = DL_I2C_getControllerStatus(AS5600_I2C_INST);
        if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
            i2c_recover();
            return false;
        }
        while ((received < length) &&
               !DL_I2C_isControllerRXFIFOEmpty(AS5600_I2C_INST)) {
            data[received++] =
                DL_I2C_receiveControllerData(AS5600_I2C_INST);
        }
    }

    if ((received != length) || !i2c_wait_transfer_done()) {
        i2c_recover();
        return false;
    }
    return true;
}

static void record_failure(void)
{
    if (g_as5600.failures < UINT8_MAX) {
        g_as5600.failures++;
    }
    if (g_as5600.failures >= AS5600_MAX_FAILURES) {
        g_as5600.healthy = false;
    }
}

bool as5600_read_status(uint8_t *status)
{
    return read_registers(AS5600_REG_STATUS, status, 1U);
}

bool as5600_read_raw(uint16_t *raw)
{
    uint8_t bytes[2];

    if ((raw == NULL) ||
        !read_registers(AS5600_REG_RAW_ANGLE, bytes, 2U)) {
        return false;
    }
    *raw = (uint16_t) ((((uint16_t) bytes[0] << 8U) | bytes[1]) & 0x0FFFU);
    return true;
}

bool as5600_init(void)
{
    uint8_t status;
    uint16_t raw;

    g_as5600 = (AS5600State) {0};
    if (!as5600_read_status(&status) || !as5600_read_raw(&raw)) {
        record_failure();
        return false;
    }

    g_as5600.status       = status;
    g_as5600.raw          = raw;
    g_as5600.lastRaw      = raw;
    g_as5600.angleRad     = (float) raw * (TWO_PI_F / 4096.0f);
    g_as5600.unwrappedRad = g_as5600.angleRad;
    g_as5600.initialized  = true;
    g_as5600.healthy      = ((status & AS5600_STATUS_MD) != 0U);
    return g_as5600.healthy;
}

bool as5600_update(float dt)
{
    uint16_t raw;
    int32_t deltaCounts;
    float deltaRad;
    float rawVelocity;
    float alpha;

    if (!isfinite(dt) || (dt <= 0.0f) || !as5600_read_raw(&raw)) {
        record_failure();
        return false;
    }

    if (!g_as5600.initialized) {
        g_as5600.lastRaw      = raw;
        g_as5600.unwrappedRad = (float) raw * (TWO_PI_F / 4096.0f);
        g_as5600.initialized  = true;
    }

    deltaCounts = (int32_t) raw - (int32_t) g_as5600.lastRaw;
    if (deltaCounts > 2048) {
        deltaCounts -= 4096;
    } else if (deltaCounts < -2048) {
        deltaCounts += 4096;
    }

    deltaRad = (float) deltaCounts * (TWO_PI_F / 4096.0f);
    rawVelocity = deltaRad / dt;
    alpha = dt / (VELOCITY_FILTER_TAU_S + dt);
    g_as5600.velocityRadS +=
        alpha * (rawVelocity - g_as5600.velocityRadS);
    g_as5600.unwrappedRad += deltaRad;
    g_as5600.angleRad = (float) raw * (TWO_PI_F / 4096.0f);
    g_as5600.lastRaw = raw;
    g_as5600.raw = raw;
    g_as5600.failures = 0U;

    if (++g_as5600.statusDivider >= 100U) {
        uint8_t status;
        g_as5600.statusDivider = 0U;
        if (!as5600_read_status(&status)) {
            record_failure();
            return false;
        }
        g_as5600.status = status;
    }

    g_as5600.healthy =
        ((g_as5600.status & AS5600_STATUS_MD) != 0U);
    return g_as5600.healthy;
}

float as5600_get_angle_rad(void)
{
    return g_as5600.angleRad;
}

float as5600_get_unwrapped_angle_rad(void)
{
    return g_as5600.unwrappedRad;
}

float as5600_get_velocity_rad_s(void)
{
    return g_as5600.velocityRadS;
}

uint16_t as5600_get_raw(void)
{
    return g_as5600.raw;
}

uint8_t as5600_get_status(void)
{
    return g_as5600.status;
}

uint8_t as5600_get_consecutive_failures(void)
{
    return g_as5600.failures;
}

bool as5600_magnet_ok(void)
{
    return ((g_as5600.status & AS5600_STATUS_MD) != 0U);
}

bool as5600_is_healthy(void)
{
    return g_as5600.initialized && g_as5600.healthy &&
           (g_as5600.failures < AS5600_MAX_FAILURES);
}
