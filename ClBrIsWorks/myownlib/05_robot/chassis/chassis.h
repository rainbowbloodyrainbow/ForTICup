#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "motor.h"

typedef struct {
    Motor *leftMotor;
    Motor *rightMotor;
    /*
     * 为 NULL 表示驱动板已在硬件上固定使能；非 NULL 时由 chassis 按安全
     * 顺序控制公共 STBY。Motor 层的 STBY 能力仍保留给其他硬件配置。
     */
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
 * driveOutput 和 turnOutput 都是有符号量。混合结果超出允许幅值时，两侧
 * 按相同比例缩放，从而保留差速比例。是否允许巡迹时倒车由 Application
 * 的运行策略决定，不在通用 chassis 中写死。
 */
void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t turnOutput,
    uint32_t nowMs);

/*
 * 用于悬空调试或上层直接给定有符号左右轮输出。超限时按相同比例缩放。
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

int16_t Chassis_GetDriveOutput(const Chassis *chassis);
int16_t Chassis_GetTurnOutput(const Chassis *chassis);
int16_t Chassis_GetLeftOutput(const Chassis *chassis);
int16_t Chassis_GetRightOutput(const Chassis *chassis);

#endif
