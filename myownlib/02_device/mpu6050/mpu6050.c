#include "mpu6050.h"

#include <stddef.h>

#define MPU6050_REG_SMPLRT_DIV     (0x19U)
#define MPU6050_REG_CONFIG         (0x1AU)
#define MPU6050_REG_GYRO_CONFIG    (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG   (0x1CU)
#define MPU6050_REG_ACCEL_XOUT_H   (0x3BU)
#define MPU6050_REG_PWR_MGMT_1     (0x6BU)
#define MPU6050_REG_WHO_AM_I       (0x75U)

#define MPU6050_WHO_AM_I_VALUE     (0x68U)
#define MPU6050_SENSOR_DATA_LENGTH (14U)
#define MPU6050_STARTUP_WAIT_MS    (100U)
#define MPU6050_WAKEUP_WAIT_MS     (100U)
#define MPU6050_GYRO_LSB_PER_DPS   (131)

typedef struct {
    uint8_t reg;
    uint8_t value;
} MPU6050_RegisterSetting;

static const MPU6050_RegisterSetting gInitSettings[] = {
    {MPU6050_REG_PWR_MGMT_1, 0x01U},
    {MPU6050_REG_CONFIG, 0x03U},
    {MPU6050_REG_SMPLRT_DIV, 0x09U},
    {MPU6050_REG_GYRO_CONFIG, 0x00U},
    {MPU6050_REG_ACCEL_CONFIG, 0x00U},
};

typedef enum {
    MPU6050_I2C_PHASE_IDLE = 0,
    MPU6050_I2C_PHASE_WRITE,
    MPU6050_I2C_PHASE_READ_REGISTER_ADDRESS,
    MPU6050_I2C_PHASE_READ_DATA,
} MPU6050_I2CPhase;

typedef enum {
    MPU6050_INIT_BOOT_WAIT = 0,
    MPU6050_INIT_WHO_AM_I_START,
    MPU6050_INIT_WHO_AM_I_WAIT,
    MPU6050_INIT_CONFIG_START,
    MPU6050_INIT_CONFIG_WAIT,
    MPU6050_INIT_WAKEUP_WAIT,
} MPU6050_InitPhase;

static I2C_Regs *gI2c;
static uint8_t gAddress;
static MPU6050_Status gStatus = MPU6050_STATUS_NOT_STARTED;
static MPU6050_InitPhase gInitPhase;
static uint32_t gNextActionMs;
static uint32_t gInitSettingIndex;

static volatile MPU6050_I2CPhase gI2cPhase =
    MPU6050_I2C_PHASE_IDLE;
static volatile bool gTransferDone;
static volatile bool gTransferError;
static volatile uint8_t gRxBuffer[MPU6050_SENSOR_DATA_LENGTH];
static volatile uint8_t gRxLength;
static volatile uint8_t gRxCount;

static MPU6050_RawData gLatestData;
static bool gNewDataAvailable;

static bool MPU6050_TimeReached(uint32_t now, uint32_t target)
{
    return ((int32_t) (now - target) >= 0);
}

static void MPU6050_DrainRXFIFO(void)
{
    while (DL_I2C_isControllerRXFIFOEmpty(gI2c) != true) {
        uint8_t value = DL_I2C_receiveControllerData(gI2c);

        if (gRxCount < gRxLength) {
            gRxBuffer[gRxCount] = value;
            gRxCount++;
        }
    }
}

static void MPU6050_RecoverI2C(void)
{
    DL_I2C_resetControllerTransfer(gI2c);
    DL_I2C_flushControllerTXFIFO(gI2c);
    DL_I2C_flushControllerRXFIFO(gI2c);
    gI2cPhase = MPU6050_I2C_PHASE_IDLE;
}

static bool MPU6050_I2CIsReady(void)
{
    uint32_t status;

    if ((gI2c == NULL) ||
        (gI2cPhase != MPU6050_I2C_PHASE_IDLE)) {
        return false;
    }

    status = DL_I2C_getControllerStatus(gI2c);

    return ((status & DL_I2C_CONTROLLER_STATUS_IDLE) != 0U);
}

static bool MPU6050_StartWriteRegister(
    uint8_t reg, uint8_t value)
{
    uint8_t txData[2];
    uint16_t written;

    if (MPU6050_I2CIsReady() == false) {
        return false;
    }

    txData[0] = reg;
    txData[1] = value;
    written = DL_I2C_fillControllerTXFIFO(gI2c, txData, 2U);

    if (written != 2U) {
        return false;
    }

    gTransferDone = false;
    gTransferError = false;
    gI2cPhase = MPU6050_I2C_PHASE_WRITE;

    DL_I2C_startControllerTransfer(
        gI2c,
        gAddress,
        DL_I2C_CONTROLLER_DIRECTION_TX,
        2U);

    return true;
}

static bool MPU6050_StartReadRegisters(
    uint8_t firstRegister, uint8_t length)
{
    uint16_t written;

    if ((length == 0U) ||
        (length > MPU6050_SENSOR_DATA_LENGTH)) {
        return false;
    }

    if (MPU6050_I2CIsReady() == false) {
        return false;
    }

    gRxLength = length;
    gRxCount = 0U;
    written = DL_I2C_fillControllerTXFIFO(
        gI2c, &firstRegister, 1U);

    if (written != 1U) {
        return false;
    }

    gTransferDone = false;
    gTransferError = false;
    gI2cPhase = MPU6050_I2C_PHASE_READ_REGISTER_ADDRESS;

    DL_I2C_startControllerTransferAdvanced(
        gI2c,
        gAddress,
        DL_I2C_CONTROLLER_DIRECTION_TX,
        1U,
        DL_I2C_CONTROLLER_START_ENABLE,
        DL_I2C_CONTROLLER_STOP_DISABLE,
        DL_I2C_CONTROLLER_ACK_DISABLE);

    return true;
}

static int16_t MPU6050_CombineSigned16(
    uint8_t highByte, uint8_t lowByte)
{
    uint16_t value = (uint16_t) (
        ((uint16_t) highByte << 8U) | (uint16_t) lowByte);

    return (int16_t) value;
}

static void MPU6050_ParseSensorData(void)
{
    gLatestData.accelX =
        MPU6050_CombineSigned16(gRxBuffer[0], gRxBuffer[1]);
    gLatestData.accelY =
        MPU6050_CombineSigned16(gRxBuffer[2], gRxBuffer[3]);
    gLatestData.accelZ =
        MPU6050_CombineSigned16(gRxBuffer[4], gRxBuffer[5]);
    gLatestData.temperature =
        MPU6050_CombineSigned16(gRxBuffer[6], gRxBuffer[7]);
    gLatestData.gyroX =
        MPU6050_CombineSigned16(gRxBuffer[8], gRxBuffer[9]);
    gLatestData.gyroY =
        MPU6050_CombineSigned16(gRxBuffer[10], gRxBuffer[11]);
    gLatestData.gyroZ =
        MPU6050_CombineSigned16(gRxBuffer[12], gRxBuffer[13]);
}

static void MPU6050_ProcessInitialization(uint32_t nowMs)
{
    if (gInitPhase == MPU6050_INIT_BOOT_WAIT) {
        if (MPU6050_TimeReached(nowMs, gNextActionMs) == true) {
            gInitPhase = MPU6050_INIT_WHO_AM_I_START;
        }
        return;
    }

    if (gInitPhase == MPU6050_INIT_WHO_AM_I_START) {
        if (MPU6050_StartReadRegisters(
                MPU6050_REG_WHO_AM_I, 1U) == true) {
            gInitPhase = MPU6050_INIT_WHO_AM_I_WAIT;
        }
        return;
    }

    if (gInitPhase == MPU6050_INIT_WHO_AM_I_WAIT) {
        if (gTransferDone == false) {
            return;
        }

        gTransferDone = false;

        if (gRxBuffer[0] != MPU6050_WHO_AM_I_VALUE) {
            gStatus = MPU6050_STATUS_ERROR;
            return;
        }

        gInitSettingIndex = 0U;
        gInitPhase = MPU6050_INIT_CONFIG_START;
        return;
    }

    if (gInitPhase == MPU6050_INIT_CONFIG_START) {
        if (MPU6050_StartWriteRegister(
                gInitSettings[gInitSettingIndex].reg,
                gInitSettings[gInitSettingIndex].value) == true) {
            gInitPhase = MPU6050_INIT_CONFIG_WAIT;
        }
        return;
    }

    if (gInitPhase == MPU6050_INIT_CONFIG_WAIT) {
        if (gTransferDone == false) {
            return;
        }

        gTransferDone = false;
        gInitSettingIndex++;

        if (gInitSettingIndex == 1U) {
            gNextActionMs = nowMs + MPU6050_WAKEUP_WAIT_MS;
            gInitPhase = MPU6050_INIT_WAKEUP_WAIT;
        } else if (gInitSettingIndex <
                   (sizeof(gInitSettings) /
                    sizeof(gInitSettings[0]))) {
            gInitPhase = MPU6050_INIT_CONFIG_START;
        } else {
            gStatus = MPU6050_STATUS_READY;
        }
        return;
    }

    if (gInitPhase == MPU6050_INIT_WAKEUP_WAIT) {
        if (MPU6050_TimeReached(nowMs, gNextActionMs) == true) {
            gInitPhase = MPU6050_INIT_CONFIG_START;
        }
    }
}

void MPU6050_Begin(
    I2C_Regs *i2c, uint8_t address, uint32_t nowMs)
{
    gI2c = i2c;
    gAddress = address;
    gTransferDone = false;
    gTransferError = false;
    gRxLength = 0U;
    gRxCount = 0U;
    gNewDataAvailable = false;
    gInitSettingIndex = 0U;
    gI2cPhase = MPU6050_I2C_PHASE_IDLE;

    if ((i2c == NULL) ||
        ((address != MPU6050_ADDRESS_AD0_LOW) &&
         (address != MPU6050_ADDRESS_AD0_HIGH))) {
        gStatus = MPU6050_STATUS_ERROR;
        return;
    }

    DL_I2C_resetControllerTransfer(gI2c);
    DL_I2C_flushControllerTXFIFO(gI2c);
    DL_I2C_flushControllerRXFIFO(gI2c);

    gNextActionMs = nowMs + MPU6050_STARTUP_WAIT_MS;
    gInitPhase = MPU6050_INIT_BOOT_WAIT;
    gStatus = MPU6050_STATUS_INITIALIZING;
}

void MPU6050_Process(uint32_t nowMs)
{
    if (gTransferError == true) {
        gTransferError = false;
        gTransferDone = false;
        MPU6050_RecoverI2C();

        if (gStatus == MPU6050_STATUS_READING) {
            gStatus = MPU6050_STATUS_READY;
        } else {
            gStatus = MPU6050_STATUS_ERROR;
        }
    }

    if (gStatus == MPU6050_STATUS_INITIALIZING) {
        MPU6050_ProcessInitialization(nowMs);
    } else if ((gStatus == MPU6050_STATUS_READING) &&
               (gTransferDone == true)) {
        gTransferDone = false;
        MPU6050_ParseSensorData();
        gNewDataAvailable = true;
        gStatus = MPU6050_STATUS_READY;
    }
}

MPU6050_Status MPU6050_GetStatus(void)
{
    return gStatus;
}

bool MPU6050_StartRead(void)
{
    if ((gStatus != MPU6050_STATUS_READY) ||
        (gNewDataAvailable == true)) {
        return false;
    }

    if (MPU6050_StartReadRegisters(
            MPU6050_REG_ACCEL_XOUT_H,
            MPU6050_SENSOR_DATA_LENGTH) == false) {
        return false;
    }

    gNewDataAvailable = false;
    gStatus = MPU6050_STATUS_READING;

    return true;
}

bool MPU6050_GetData(MPU6050_RawData *data)
{
    if ((data == NULL) || (gNewDataAvailable == false)) {
        return false;
    }

    *data = gLatestData;
    gNewDataAvailable = false;

    return true;
}

void MPU6050_HandleI2CInterrupt(void)
{
    switch (DL_I2C_getPendingInterrupt(gI2c)) {
        case DL_I2C_IIDX_CONTROLLER_TX_DONE:
            if (gI2cPhase ==
                MPU6050_I2C_PHASE_READ_REGISTER_ADDRESS) {
                gI2cPhase = MPU6050_I2C_PHASE_READ_DATA;

                DL_I2C_startControllerTransferAdvanced(
                    gI2c,
                    gAddress,
                    DL_I2C_CONTROLLER_DIRECTION_RX,
                    gRxLength,
                    DL_I2C_CONTROLLER_START_ENABLE,
                    DL_I2C_CONTROLLER_STOP_ENABLE,
                    DL_I2C_CONTROLLER_ACK_DISABLE);
            } else if (gI2cPhase == MPU6050_I2C_PHASE_WRITE) {
                gI2cPhase = MPU6050_I2C_PHASE_IDLE;
                gTransferDone = true;
            }
            break;

        case DL_I2C_IIDX_CONTROLLER_RXFIFO_TRIGGER:
            MPU6050_DrainRXFIFO();
            break;

        case DL_I2C_IIDX_CONTROLLER_RX_DONE:
            MPU6050_DrainRXFIFO();
            gI2cPhase = MPU6050_I2C_PHASE_IDLE;

            if (gRxCount == gRxLength) {
                gTransferDone = true;
            } else {
                gTransferError = true;
            }
            break;

        case DL_I2C_IIDX_CONTROLLER_NACK:
        case DL_I2C_IIDX_CONTROLLER_ARBITRATION_LOST:
            gTransferDone = false;
            gTransferError = true;
            gI2cPhase = MPU6050_I2C_PHASE_IDLE;
            break;

        default:
            break;
    }
}

int32_t MPU6050_GyroRawToCentiDps(int32_t rawValue)
{
    return (rawValue * 100) / MPU6050_GYRO_LSB_PER_DPS;
}
