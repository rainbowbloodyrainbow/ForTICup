#include "adc.h"

#include <stddef.h>

#define ADC_12_BIT_MAX_VALUE (4095U)
#define ADC_SEQUENCE8_TIMEOUT_ITERATIONS (100000U)

static void ADC_ClearSequenceStatus(ADC12_Regs *adc)
{
    DL_ADC12_clearInterruptStatus(
        adc,
        DL_ADC12_INTERRUPT_MEM7_RESULT_LOADED);
}

static void ADC_StartSequence(ADC12_Regs *adc)
{
    DL_ADC12_startConversion(adc);
}

static bool ADC_IsSequenceComplete(ADC12_Regs *adc)
{
    return DL_ADC12_getRawInterruptStatus(
               adc,
               DL_ADC12_INTERRUPT_MEM7_RESULT_LOADED) != 0U;
}

static void ADC_ReadSequence8Results(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE8_COUNT])
{
    uint32_t index;

    for (index = 0U; index < ADC_SEQUENCE8_COUNT; index++) {
        result[index] = DL_ADC12_getMemResult(
            adc, (DL_ADC12_MEM_IDX) index);
    }
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
    uint32_t remaining;

    if ((adc == NULL) || (result == NULL)) {
        return ADC_STATUS_INVALID_ARGUMENT;
    }

    ADC_ClearSequenceStatus(adc);
    ADC_StartSequence(adc);

    remaining = ADC_SEQUENCE8_TIMEOUT_ITERATIONS;
    while (!ADC_IsSequenceComplete(adc)) {
        if (remaining == 0U) {
            DL_ADC12_stopConversion(adc);
            DL_ADC12_enableConversions(adc);
            return ADC_STATUS_TIMEOUT;
        }
        remaining--;
    }

    ADC_ReadSequence8Results(adc, result);
    ADC_ClearSequenceStatus(adc);

    /*
     * 非重复序列完成后 ENC 会被硬件清除。TI 的序列转换示例同样在每次
     * 读取后重新使能，以便下一次软件触发。
     */
    DL_ADC12_enableConversions(adc);

    return ADC_STATUS_OK;
}
