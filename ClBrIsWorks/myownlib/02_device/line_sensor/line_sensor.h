#ifndef LINE_SENSOR_H
#define LINE_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#define LINE_SENSOR_COUNT (8U)
#define LINE_SENSOR_STRENGTH_MAX (1000U)

typedef struct {
    /*
     * physical[i] = adcRaw[channelMap[i]]。
     * physical[0] 必须对应车体最左侧探头。
     */
    uint8_t channelMap[LINE_SENSOR_COUNT];
    uint16_t backgroundValue[LINE_SENSOR_COUNT];
    uint16_t lineValue[LINE_SENSOR_COUNT];
    int16_t positionWeight[LINE_SENSOR_COUNT];
    uint16_t minimumCalibrationRange;
    uint32_t minimumTotalStrength;
} LineSensor_Config;

typedef struct {
    uint16_t raw[LINE_SENSOR_COUNT];
    uint16_t strength[LINE_SENSOR_COUNT];
    int32_t position;
    uint32_t totalStrength;
    bool valid;
} LineSensor_Result;

typedef struct {
    LineSensor_Config config;
    LineSensor_Result result;
    int32_t lastValidPosition;
    bool initialized;
} LineSensor;

bool LineSensor_Init(
    LineSensor *sensor,
    const LineSensor_Config *config);
bool LineSensor_IsConfigValid(
    const LineSensor_Config *config);
bool LineSensor_ProcessRaw(
    LineSensor *sensor,
    const uint16_t adcRaw[LINE_SENSOR_COUNT]);
const LineSensor_Result *LineSensor_GetResult(
    const LineSensor *sensor);

#endif
