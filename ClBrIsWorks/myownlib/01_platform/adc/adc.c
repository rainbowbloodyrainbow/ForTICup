#include "adc.h"

#include <stddef.h>

#define ADC_12_BIT_MAX_VALUE (4095U)
#define ADC_SEQUENCE_TIMEOUT_ITERATIONS (100000U)

static void ADC_ClearSequenceStatus(
    ADC12_Regs *adc, uint32_t completionInterrupt)
{
    DL_ADC12_clearInterruptStatus(
        adc, completionInterrupt);
}

static void ADC_StartSequence(ADC12_Regs *adc)
{
    DL_ADC12_startConversion(adc);
}

static bool ADC_IsSequenceComplete(
    ADC12_Regs *adc, uint32_t completionInterrupt)
{
    return DL_ADC12_getRawInterruptStatus(
               adc, completionInterrupt) != 0U;
}

static void ADC_ReadSequenceResults(
    ADC12_Regs *adc,
    uint16_t *result,
    uint32_t resultCount)
{
    uint32_t index;

    for (index = 0U; index < resultCount; index++) {
        result[index] = DL_ADC12_getMemResult(
            adc, (DL_ADC12_MEM_IDX) index);
    }
}

static ADC_Status ADC_ReadSequence(
    ADC12_Regs *adc,
    uint16_t *result,
    uint32_t resultCount,
    uint32_t completionInterrupt)
{
    uint32_t remaining;

    if ((adc == NULL) ||
        (result == NULL) ||
        (resultCount == 0U)) {
        return ADC_STATUS_INVALID_ARGUMENT;
    }

    ADC_ClearSequenceStatus(adc, completionInterrupt);
    ADC_StartSequence(adc);

    remaining = ADC_SEQUENCE_TIMEOUT_ITERATIONS;
    while (!ADC_IsSequenceComplete(
        adc, completionInterrupt)) {
        if (remaining == 0U) {
            DL_ADC12_stopConversion(adc);
            DL_ADC12_enableConversions(adc);
            return ADC_STATUS_TIMEOUT;
        }
        remaining--;
    }

    ADC_ReadSequenceResults(adc, result, resultCount);
    ADC_ClearSequenceStatus(adc, completionInterrupt);

    /*
     * 非重复序列结束后重新使能 ENC，允许下一次软件触发。
     */
    DL_ADC12_enableConversions(adc);
    return ADC_STATUS_OK;
}

uint16_t ADC_ReadRaw(ADC12_Regs *adc)
{
    uint16_t result;

    DL_ADC12_startConversion(adc);

    while (DL_ADC12_getRawInterruptStatus(
               adc,
               DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0U) {
    }

    DL_ADC12_clearInterruptStatus(
        adc,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    result = DL_ADC12_getMemResult(
        adc,
        DL_ADC12_MEM_IDX_0);

    DL_ADC12_enableConversions(adc);

    return result;
}

uint32_t ADC_RawToMillivolts(
    uint16_t rawValue, uint32_t referenceMillivolts)
{
    return ((uint32_t) rawValue * referenceMillivolts) /
        ADC_12_BIT_MAX_VALUE;
}

ADC_Status ADC_ReadSequence8(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE8_COUNT])
{
    return ADC_ReadSequence(
        adc,
        result,
        ADC_SEQUENCE8_COUNT,
        DL_ADC12_INTERRUPT_MEM7_RESULT_LOADED);
}

ADC_Status ADC_ReadSequence5(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE5_COUNT])
{
    return ADC_ReadSequence(
        adc,
        result,
        ADC_SEQUENCE5_COUNT,
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
}
