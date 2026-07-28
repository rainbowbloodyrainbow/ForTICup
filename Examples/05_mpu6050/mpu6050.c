#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "hc05.h"
#include "mpu6050.h"

#define SAMPLE_PERIOD_MS          (10U)
#define CALIBRATION_SAMPLE_COUNT  (200U)
#define PRINT_INTERVAL_SAMPLES    (10U)

typedef enum {
    APP_WAITING_FOR_MPU6050 = 0,
    APP_CALIBRATING,
    APP_RUNNING,
    APP_ERROR,
} App_State;

/* 定时器每10 ms增加一次，作为模块和应用共同使用的毫秒时间。 */
volatile uint32_t gMilliseconds;

/* 可在调试器 Watch 窗口中直接观察的数据。 */
volatile int16_t gGyroZRaw;
volatile int32_t gGyroZOffset;
volatile int32_t gGyroZCentiDps;
volatile int64_t gTurnAngleCentiDeg;
volatile uint32_t gMissedSamples;

static App_State gAppState = APP_WAITING_FOR_MPU6050;
static uint32_t gLastSampleStartMs;
static uint32_t gCalibrationSamples;
static int64_t gCalibrationSum;
static int64_t gAngleRawSampleSum;
static uint32_t gPrintSampleCounter;

static void App_SendUint64(uint64_t value)
{
    char digits[20];
    uint32_t count = 0U;

    if (value == 0U) {
        HC05_SendByte(HC05_UART_INST, (uint8_t) '0');
        return;
    }

    while (value > 0U) {
        digits[count] = (char) ('0' + (value % 10U));
        count++;
        value /= 10U;
    }

    while (count > 0U) {
        count--;
        HC05_SendByte(
            HC05_UART_INST, (uint8_t) digits[count]);
    }
}

static void App_SendSignedHundredths(int64_t value)
{
    uint64_t magnitude;
    uint64_t fraction;

    if (value < 0) {
        HC05_SendByte(HC05_UART_INST, (uint8_t) '-');
        magnitude = (uint64_t) (-(value + 1));
        magnitude++;
    } else {
        HC05_SendByte(HC05_UART_INST, (uint8_t) '+');
        magnitude = (uint64_t) value;
    }

    fraction = magnitude % 100U;
    App_SendUint64(magnitude / 100U);
    HC05_SendByte(HC05_UART_INST, (uint8_t) '.');
    HC05_SendByte(
        HC05_UART_INST,
        (uint8_t) ('0' + (fraction / 10U)));
    HC05_SendByte(
        HC05_UART_INST,
        (uint8_t) ('0' + (fraction % 10U)));
}

static void App_PrintMeasurements(void)
{
    int32_t gyroCentiDps = gGyroZCentiDps;
    int64_t angleCentiDeg = gTurnAngleCentiDeg;

    HC05_SendString(HC05_UART_INST, "GZ=");
    App_SendSignedHundredths(gyroCentiDps);
    HC05_SendString(HC05_UART_INST, " dps, Angle=");
    App_SendSignedHundredths(angleCentiDeg);
    HC05_SendString(HC05_UART_INST, " deg\r\n");
}

static void App_StartCalibration(void)
{
    gCalibrationSamples = 0U;
    gCalibrationSum = 0;
    gAngleRawSampleSum = 0;
    gTurnAngleCentiDeg = 0;
    gGyroZCentiDps = 0;
    gLastSampleStartMs = gMilliseconds;
    gAppState = APP_CALIBRATING;

    HC05_SendString(
        HC05_UART_INST,
        "Keep the car still: collecting 200 samples "
        "for Z-axis calibration.\r\n");
}

static void App_ProcessSensorData(
    const MPU6050_RawData *sensorData)
{
    int32_t correctedRaw;

    gGyroZRaw = sensorData->gyroZ;

    if (gAppState == APP_CALIBRATING) {
        gCalibrationSum += gGyroZRaw;
        gCalibrationSamples++;

        if (gCalibrationSamples >= CALIBRATION_SAMPLE_COUNT) {
            gGyroZOffset =
                (int32_t) (gCalibrationSum /
                    CALIBRATION_SAMPLE_COUNT);
            gAngleRawSampleSum = 0;
            gTurnAngleCentiDeg = 0;
            gPrintSampleCounter = 0U;
            gAppState = APP_RUNNING;

            HC05_SendString(
                HC05_UART_INST,
                "Calibration complete. Z offset=");
            HC05_SendInt32(HC05_UART_INST, gGyroZOffset);
            HC05_SendString(HC05_UART_INST, " LSB\r\n");
            HC05_SendString(
                HC05_UART_INST,
                "Streaming at 100 Hz. Send 'z' to zero angle, "
                "'r' to recalibrate, 'p' to print.\r\n");
        }

        return;
    }

    correctedRaw = (int32_t) gGyroZRaw - gGyroZOffset;
    gGyroZCentiDps =
        MPU6050_GyroRawToCentiDps(correctedRaw);

    /*
     * +/-250 dps、100 Hz采样时：
     *   角度(0.01度) += correctedRaw / 131
     * 先累加原始量，避免每次整数除法造成累计截断。
     */
    gAngleRawSampleSum += correctedRaw;
    gTurnAngleCentiDeg = gAngleRawSampleSum / 131;

    gPrintSampleCounter++;
    if (gPrintSampleCounter >= PRINT_INTERVAL_SAMPLES) {
        gPrintSampleCounter = 0U;
        App_PrintMeasurements();
    }
}

static void App_HandleUARTCommands(void)
{
    uint8_t command;

    while (HC05_ReadByte(&command)) {
        if ((command == (uint8_t) 'z') ||
            (command == (uint8_t) 'Z')) {
            gAngleRawSampleSum = 0;
            gTurnAngleCentiDeg = 0;
            HC05_SendString(
                HC05_UART_INST,
                "Angle reset to 0.00 deg.\r\n");
        } else if ((command == (uint8_t) 'r') ||
                   (command == (uint8_t) 'R')) {
            if ((gAppState == APP_RUNNING) ||
                (gAppState == APP_CALIBRATING)) {
                App_StartCalibration();
            }
        } else if ((command == (uint8_t) 'p') ||
                   (command == (uint8_t) 'P')) {
            App_PrintMeasurements();
        }
    }
}

static void App_HandleMPU6050Status(void)
{
    MPU6050_Status status = MPU6050_GetStatus();

    if ((status == MPU6050_STATUS_ERROR) &&
        (gAppState != APP_ERROR)) {
        gAppState = APP_ERROR;
        HC05_SendString(
            HC05_UART_INST,
            "ERROR: MPU6050 initialization or I2C failed.\r\n");
        return;
    }

    if ((gAppState == APP_WAITING_FOR_MPU6050) &&
        (status == MPU6050_STATUS_READY)) {
        HC05_SendString(
            HC05_UART_INST,
            "MPU6050 configuration complete.\r\n");
        App_StartCalibration();
    }
}

static void App_RunSampling(void)
{
    MPU6050_RawData sensorData;
    uint32_t now;
    uint32_t elapsedMs;
    uint32_t elapsedPeriods;

    if ((gAppState != APP_CALIBRATING) &&
        (gAppState != APP_RUNNING)) {
        return;
    }

    if (MPU6050_GetData(&sensorData)) {
        App_ProcessSensorData(&sensorData);
    }

    if (MPU6050_GetStatus() != MPU6050_STATUS_READY) {
        return;
    }

    now = gMilliseconds;
    elapsedMs = now - gLastSampleStartMs;

    if (elapsedMs < SAMPLE_PERIOD_MS) {
        return;
    }

    if (MPU6050_StartRead()) {
        elapsedPeriods = elapsedMs / SAMPLE_PERIOD_MS;

        if (elapsedPeriods > 1U) {
            gMissedSamples += elapsedPeriods - 1U;
        }

        gLastSampleStartMs = now;
    }
}

int main(void)
{
    SYSCFG_DL_init();
    HC05_ResetReceiver();

    MPU6050_Begin(
        MPU6050_I2C_INST,
        MPU6050_ADDRESS_AD0_LOW,
        gMilliseconds);

    HC05_SendString(
        HC05_UART_INST,
        "\r\nMSPM0 MPU6050 monitor starting.\r\n");
    HC05_SendString(
        HC05_UART_INST,
        "I2C interrupt mode, 10 ms timer trigger, "
        "HC-05 at 115200 baud.\r\n");

    NVIC_ClearPendingIRQ(MPU6050_I2C_INST_INT_IRQN);
    NVIC_EnableIRQ(MPU6050_I2C_INST_INT_IRQN);

    NVIC_ClearPendingIRQ(HC05_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(HC05_UART_INST_INT_IRQN);

    NVIC_ClearPendingIRQ(SAMPLE_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(SAMPLE_TIMER_INST_INT_IRQN);

    DL_TimerG_startCounter(SAMPLE_TIMER_INST);

    while (1) {
        MPU6050_Process(gMilliseconds);
        App_HandleMPU6050Status();
        App_HandleUARTCommands();
        App_RunSampling();

        /*
         * CPU等待定时器、I2C或UART中断。
         * I2C传输期间不使用 while 循环等待外设状态。
         */
        __WFI();
    }
}

void SAMPLE_TIMER_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(SAMPLE_TIMER_INST)) {
        case DL_TIMER_IIDX_ZERO:
            gMilliseconds += SAMPLE_PERIOD_MS;
            break;

        default:
            break;
    }
}

void MPU6050_I2C_INST_IRQHandler(void)
{
    MPU6050_HandleI2CInterrupt();
}

void HC05_UART_INST_IRQHandler(void)
{
    HC05_HandleRxInterrupt(HC05_UART_INST);
}
