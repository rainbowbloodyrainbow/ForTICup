#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#include <ti/driverlib/dl_adc12.h>

/*
 * ADC 实例、输入通道、参考电压来源、采样时间和时钟应先在应用工程的
 * SysConfig 中配置，并由 SYSCFG_DL_init() 初始化。
 *
 * 当前接口完成一次阻塞式单通道转换，并读取 ADCMEM0。
 */
uint16_t ADC_ReadRaw(ADC12_Regs *adc);

/*
 * 将 12 位 ADC 原始值换算为毫伏。
 * referenceMillivolts 应传入本次转换实际使用的参考电压，单位为 mV。
 */
uint32_t ADC_RawToMillivolts(
    uint16_t rawValue, uint32_t referenceMillivolts);

#endif
