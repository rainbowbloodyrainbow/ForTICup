#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "motor.h"
#include "servo.h"

typedef struct {
    Motor *leftMotor;
    Motor *rightMotor;
    Motor_Standby *standby;
    Servo *steeringServo;
    uint16_t maximumDriveOutput;
    uint16_t maximumSteeringCommand;
    uint16_t leftOpenLoopScalePermille;
    uint16_t rightOpenLoopScalePermille;
} Chassis_Config;

typedef struct {
    Chassis_Config config;
    int16_t driveOutput;
    int16_t steeringCommand;
    bool enabled;
    bool initialized;
} Chassis;

bool Chassis_Init(
    Chassis *chassis, const Chassis_Config *config);
bool Chassis_Enable(
    Chassis *chassis, uint32_t nowMs);
void Chassis_Disable(
    Chassis *chassis, uint32_t nowMs);
void Chassis_SetOpenLoop(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t steeringCommand,
    uint32_t nowMs);
void Chassis_Brake(
    Chassis *chassis, uint32_t nowMs);
void Chassis_Coast(
    Chassis *chassis, uint32_t nowMs);
void Chassis_CenterSteering(Chassis *chassis);
void Chassis_Process(
    Chassis *chassis, uint32_t nowMs);

#endif
