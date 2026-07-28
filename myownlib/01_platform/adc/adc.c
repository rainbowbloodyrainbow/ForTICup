#include "adc.h"

#define ADC_12_BIT_MAX_VALUE (4095U)

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
