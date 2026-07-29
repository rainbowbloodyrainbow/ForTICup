#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#include <ti/driverlib/dl_i2c.h>

#define MPU6050_ADDRESS_AD0_LOW  (0x68U)
#define MPU6050_ADDRESS_AD0_HIGH (0x69U)

typedef struct {
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    int16_t temperature;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
} MPU6050_RawData;

typedef enum {
    MPU6050_STATUS_NOT_STARTED = 0,
    MPU6050_STATUS_INITIALIZING,
    MPU6050_STATUS_READY,
    MPU6050_STATUS_READING,
    MPU6050_STATUS_ERROR,
} MPU6050_Status;

/*
 * I2C 实例、时钟、SDA/SCL 引脚和所需控制器中断应先在应用工程的
 * SysConfig 中配置，并由 SYSCFG_DL_init() 初始化。
 *
 * Begin() 只启动非阻塞初始化。应用应在主循环持续调用 Process()，
 * 并传入单调递增的毫秒时间。
 *
 * 当前固定配置：
 *   采样率       100 Hz
 *   数字低通滤波 CONFIG = 0x03
 *   陀螺仪量程   +/-250 dps
 *   加速度计量程 +/-2 g
 */
void MPU6050_Begin(
    I2C_Regs *i2c, uint8_t address, uint32_t nowMs);
void MPU6050_Process(uint32_t nowMs);
MPU6050_Status MPU6050_GetStatus(void);

/*
 * StartRead() 启动一次从 ACCEL_XOUT_H 开始的 14 字节异步读取。
 * GetData() 只在有一份尚未取走的新数据时返回 true。
 */
bool MPU6050_StartRead(void);
bool MPU6050_GetData(MPU6050_RawData *data);

/*
 * 应用工程自己的 I2C IRQHandler 必须调用此函数。
 * 当前模块内部只保存一套状态，因此只支持一个 MPU6050 实例，
 * 并要求该传感器独占传入的 I2C 控制器。
 */
void MPU6050_HandleI2CInterrupt(void);

/*
 * 按当前 +/-250 dps 量程，将原始值换算为 0.01 dps。
 * rawValue 可以先由应用减去静止零偏后再传入。
 */
int32_t MPU6050_GyroRawToCentiDps(int32_t rawValue);

#endif
