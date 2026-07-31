#include "line_sensor.h"

#include <stddef.h>

static bool LineSensor_IsChannelMapValid(
    const uint8_t channelMap[LINE_SENSOR_COUNT])
{
    bool used[LINE_SENSOR_COUNT] = {false};
    uint32_t index;
    uint8_t channel;

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        channel = channelMap[index];
        if ((channel >= LINE_SENSOR_COUNT) || used[channel]) {
            return false;
        }
        used[channel] = true;
    }

    return true;
}

static uint16_t LineSensor_NormalizeOne(
    uint16_t raw,
    uint16_t background,
    uint16_t line)
{
    int32_t numerator;
    int32_t denominator;
    int32_t strength;

    numerator =
        (int32_t) raw - (int32_t) background;
    denominator =
        (int32_t) line - (int32_t) background;
    strength =
        (numerator * (int32_t) LINE_SENSOR_STRENGTH_MAX) /
        denominator;

    if (strength < 0) {
        strength = 0;
    } else if (strength >
        (int32_t) LINE_SENSOR_STRENGTH_MAX) {
        strength = (int32_t) LINE_SENSOR_STRENGTH_MAX;
    }

    return (uint16_t) strength;
}

static uint16_t LineSensor_ThresholdOne(
    uint16_t raw,
    uint16_t threshold,
    bool lineIsHigh)
{
    bool lineDetected;

    if (lineIsHigh) {
        lineDetected = (raw >= threshold);
    } else {
        lineDetected = (raw <= threshold);
    }

    return lineDetected ?
        LINE_SENSOR_STRENGTH_MAX : 0U;
}

static void LineSensor_MapRaw(
    const LineSensor_Config *config,
    const uint16_t adcRaw[LINE_SENSOR_COUNT],
    uint16_t physicalRaw[LINE_SENSOR_COUNT])
{
    uint32_t index;

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        physicalRaw[index] =
            adcRaw[config->channelMap[index]];
    }
}

static void LineSensor_CalculatePosition(LineSensor *sensor)
{
    int64_t weightedSum = 0;
    uint32_t totalStrength = 0U;
    uint32_t index;

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        weightedSum +=
            (int64_t) sensor->result.strength[index] *
            sensor->config.positionWeight[index];
        totalStrength += sensor->result.strength[index];
    }

    sensor->result.totalStrength = totalStrength;
    if ((totalStrength == 0U) ||
        (totalStrength <
            sensor->config.minimumTotalStrength)) {
        sensor->result.valid = false;
        sensor->result.position = sensor->lastValidPosition;
        return;
    }

    sensor->result.position =
        (int32_t) (weightedSum / (int64_t) totalStrength);
    sensor->result.valid = true;
    sensor->lastValidPosition = sensor->result.position;
}

bool LineSensor_IsConfigValid(
    const LineSensor_Config *config)
{
    uint32_t index;
    int32_t range;

    if ((config == NULL) ||
        (config->minimumTotalStrength == 0U) ||
        (config->minimumTotalStrength >
            (LINE_SENSOR_COUNT *
                LINE_SENSOR_STRENGTH_MAX)) ||
        !LineSensor_IsChannelMapValid(config->channelMap)) {
        return false;
    }

    if (config->mode ==
        LINE_SENSOR_MODE_BINARY_THRESHOLD) {
        return true;
    }
    if ((config->mode != LINE_SENSOR_MODE_CALIBRATED) ||
        (config->minimumCalibrationRange == 0U)) {
        return false;
    }

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        range =
            (int32_t) config->lineValue[index] -
            (int32_t) config->backgroundValue[index];
        if (range < 0) {
            range = -range;
        }
        if (range <
            (int32_t) config->minimumCalibrationRange) {
            return false;
        }
    }

    return true;
}

bool LineSensor_Init(
    LineSensor *sensor,
    const LineSensor_Config *config)
{
    uint32_t index;

    if ((sensor == NULL) ||
        !LineSensor_IsConfigValid(config)) {
        return false;
    }

    sensor->config = *config;
    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        sensor->result.raw[index] = 0U;
        sensor->result.strength[index] = 0U;
    }
    sensor->result.position = 0;
    sensor->result.totalStrength = 0U;
    sensor->result.valid = false;
    sensor->lastValidPosition = 0;
    sensor->initialized = true;

    return true;
}

bool LineSensor_ProcessRaw(
    LineSensor *sensor,
    const uint16_t adcRaw[LINE_SENSOR_COUNT])
{
    uint32_t index;

    if ((sensor == NULL) ||
        !sensor->initialized ||
        (adcRaw == NULL)) {
        return false;
    }

    LineSensor_MapRaw(
        &sensor->config, adcRaw, sensor->result.raw);

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        if (sensor->config.mode ==
            LINE_SENSOR_MODE_BINARY_THRESHOLD) {
            sensor->result.strength[index] =
                LineSensor_ThresholdOne(
                    sensor->result.raw[index],
                    sensor->config.thresholdValue[index],
                    sensor->config.lineIsHigh[index]);
        } else {
            sensor->result.strength[index] =
                LineSensor_NormalizeOne(
                    sensor->result.raw[index],
                    sensor->config.backgroundValue[index],
                    sensor->config.lineValue[index]);
        }
    }

    LineSensor_CalculatePosition(sensor);
    return true;
}

const LineSensor_Result *LineSensor_GetResult(
    const LineSensor *sensor)
{
    if ((sensor == NULL) || !sensor->initialized) {
        return NULL;
    }

    return &sensor->result;
}
