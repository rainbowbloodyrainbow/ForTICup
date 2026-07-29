#ifndef LINE_CONTROL_H
#define LINE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "line_sensor.h"
#include "pid.h"

typedef enum {
    LINE_CONTROL_TRACKING = 0,
    LINE_CONTROL_HOLDING,
    LINE_CONTROL_LINE_LOST,
    LINE_CONTROL_INVALID_ARGUMENT
} LineControl_Status;

typedef struct {
    PID_Config steeringPid;
    float positionFullScale;
    int16_t maximumSteeringCommand;
    bool steeringInverted;
    uint8_t maximumInvalidFrames;
} LineControl_Config;

typedef struct {
    LineControl_Config config;
    PID steeringPid;
    int16_t steeringCommand;
    uint8_t consecutiveInvalidFrames;
    LineControl_Status status;
    bool initialized;
} LineControl;

bool LineControl_Init(
    LineControl *control,
    const LineControl_Config *config);
void LineControl_Reset(LineControl *control);
LineControl_Status LineControl_Update(
    LineControl *control,
    const LineSensor_Result *line,
    float dtSeconds);
int16_t LineControl_GetSteeringCommand(
    const LineControl *control);
LineControl_Status LineControl_GetStatus(
    const LineControl *control);
uint8_t LineControl_GetInvalidFrameCount(
    const LineControl *control);

#endif
