#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#include <ti/driverlib/dl_adc12.h>

#define ADC_SEQUENCE5_COUNT (5U)
#define ADC_SEQUENCE8_COUNT (8U)

typedef enum {
    ADC_STATUS_OK = 0,
    ADC_STATUS_INVALID_ARGUMENT,
    ADC_STATUS_TIMEOUT
} ADC_Status;

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

/*
 * 软件触发一次 ADCMEM0～ADCMEM7 序列转换并依次返回八路结果。
 * 当前接口面向 SysConfig 中起始地址 0、结束地址 7、非重复序列模式。
 * 等待采用有限轮询；硬件未在期限内装载 ADCMEM7 时返回 TIMEOUT。
 */
ADC_Status ADC_ReadSequence8(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE8_COUNT]);

/*
 * 软件触发一次 ADCMEM0～ADCMEM4 序列转换。result[0]～result[4]
 * 依次对应 SysConfig 中 MEM0～MEM4 的物理通道。
 */
ADC_Status ADC_ReadSequence5(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE5_COUNT]);

#endif
