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
    PID_Config turnPid;
    float positionFullScale;
    int16_t maximumTurnCommand;
    bool turnInverted;
    uint8_t maximumInvalidFrames;

    /*
     * 启用后按五路二值强度使用 2、1、0、-1、-2 权重输出分级转向；
     * 关闭时仍使用连续质心位置和 PID。
     */
    bool binaryPatternEnabled;
    int16_t binaryCorrectionCommand;
    int16_t binarySharpCommand;
} LineControl_Config;

typedef struct {
    LineControl_Config config;
    PID turnPid;
    int16_t turnCommand;
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
int16_t LineControl_GetTurnCommand(
    const LineControl *control);
LineControl_Status LineControl_GetStatus(
    const LineControl *control);
uint8_t LineControl_GetInvalidFrameCount(
    const LineControl *control);

#endif
