#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "motor.h"

typedef struct {
    Motor *leftMotor;
    Motor *rightMotor;
    Motor_Standby *standby;
    uint16_t maximumDriveOutput;
    uint16_t maximumTurnOutput;
    uint16_t leftOpenLoopScalePermille;
    uint16_t rightOpenLoopScalePermille;
} Chassis_Config;

typedef struct {
    Chassis_Config config;
    int16_t driveOutput;
    int16_t turnOutput;
    int16_t leftOutput;
    int16_t rightOutput;
    bool enabled;
    bool initialized;
} Chassis;

bool Chassis_Init(
    Chassis *chassis, const Chassis_Config *config);
bool Chassis_Enable(
    Chassis *chassis, uint32_t nowMs);
void Chassis_Disable(
    Chassis *chassis, uint32_t nowMs);

/*
 * turnOutput > 0 表示左转：
 *     left = drive - turn
 *     right = drive + turn
 *
 * 当前巡迹安全策略禁止车轮反转。混合结果小于 0 时按 0 输出，
 * 大于 maximumDriveOutput 时按 maximumDriveOutput 输出。
 */
void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t turnOutput,
    uint32_t nowMs);

/*
 * 用于悬空调试左右轮。两个输入同样只允许 0～maximumDriveOutput。
 */
void Chassis_SetWheelOutputs(
    Chassis *chassis,
    int16_t leftOutput,
    int16_t rightOutput,
    uint32_t nowMs);

void Chassis_Brake(
    Chassis *chassis, uint32_t nowMs);
void Chassis_Coast(
    Chassis *chassis, uint32_t nowMs);
void Chassis_Process(
    Chassis *chassis, uint32_t nowMs);

#endif
