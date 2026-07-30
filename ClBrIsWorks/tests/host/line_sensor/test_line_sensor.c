#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "line_sensor.h"

static LineSensor_Config MakeConfig(bool inversePolarity)
{
    static const int16_t weights[LINE_SENSOR_COUNT] = {
        -2000, -1000, 0, 1000, 2000
    };
    LineSensor_Config config;
    uint32_t index;

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        config.channelMap[index] = (uint8_t) index;
        config.backgroundValue[index] =
            inversePolarity ? 3000U : 1000U;
        config.lineValue[index] =
            inversePolarity ? 1000U : 3000U;
        config.positionWeight[index] = weights[index];
    }
    config.minimumCalibrationRange = 100U;
    config.minimumTotalStrength = 100U;
    return config;
}

static void Fill(
    uint16_t values[LINE_SENSOR_COUNT], uint16_t value)
{
    uint32_t index;

    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        values[index] = value;
    }
}

static void TestConfigurationValidation(void)
{
    LineSensor_Config config = MakeConfig(false);

    config.channelMap[4] = config.channelMap[3];
    assert(!LineSensor_IsConfigValid(&config));

    config = MakeConfig(false);
    config.lineValue[4] =
        config.backgroundValue[4] + 99U;
    assert(!LineSensor_IsConfigValid(&config));
}

static void TestBothPolaritiesAndSaturation(void)
{
    LineSensor sensor = {0};
    LineSensor_Config config = MakeConfig(false);
    uint16_t raw[LINE_SENSOR_COUNT];
    const LineSensor_Result *result;

    assert(LineSensor_Init(&sensor, &config));
    Fill(raw, 1000U);
    raw[0] = 2000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    result = LineSensor_GetResult(&sensor);
    assert(result->strength[0] == 500U);

    raw[0] = 4000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    assert(result->strength[0] ==
        LINE_SENSOR_STRENGTH_MAX);

    config = MakeConfig(true);
    assert(LineSensor_Init(&sensor, &config));
    Fill(raw, 3000U);
    raw[4] = 2000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    result = LineSensor_GetResult(&sensor);
    assert(result->strength[4] == 500U);

    raw[4] = 0U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    assert(result->strength[4] ==
        LINE_SENSOR_STRENGTH_MAX);
}

static void TestPositionsAndInvalidRetention(void)
{
    LineSensor sensor = {0};
    LineSensor_Config config = MakeConfig(false);
    uint16_t raw[LINE_SENSOR_COUNT];
    const LineSensor_Result *result;

    assert(LineSensor_Init(&sensor, &config));

    Fill(raw, 1000U);
    raw[2] = 3000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    result = LineSensor_GetResult(&sensor);
    assert(result->valid);
    assert(result->position == 0);

    Fill(raw, 1000U);
    raw[0] = 3000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    assert(result->valid);
    assert(result->position == -2000);

    Fill(raw, 1000U);
    assert(LineSensor_ProcessRaw(&sensor, raw));
    assert(!result->valid);
    assert(result->position == -2000);

    raw[4] = 3000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    assert(result->valid);
    assert(result->position == 2000);
}

static void TestChannelMapping(void)
{
    LineSensor sensor = {0};
    LineSensor_Config config = MakeConfig(false);
    uint16_t raw[LINE_SENSOR_COUNT];
    const LineSensor_Result *result;
    uint8_t temporary;

    temporary = config.channelMap[0];
    config.channelMap[0] = config.channelMap[4];
    config.channelMap[4] = temporary;
    assert(LineSensor_Init(&sensor, &config));

    Fill(raw, 1000U);
    raw[4] = 3000U;
    assert(LineSensor_ProcessRaw(&sensor, raw));
    result = LineSensor_GetResult(&sensor);
    assert(result->position == -2000);
}

int main(void)
{
    TestConfigurationValidation();
    TestBothPolaritiesAndSaturation();
    TestPositionsAndInvalidRetention();
    TestChannelMapping();

    puts("line_sensor host tests passed");
    return 0;
}
