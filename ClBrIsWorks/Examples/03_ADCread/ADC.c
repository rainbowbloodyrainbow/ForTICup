#include "ti_msp_dl_config.h"

#include "adc.h"
#include "hc05.h"

#define ADC_PRINT_INTERVAL_MS (500U)

/*
 * PA27 -> ADC0 channel 0 -> ADCMEM0
 *
 * 这两个变量可以在调试器的 Expressions/Watch 窗口中观察：
 * gAdcValue:   12 位 ADC 原始值，范围为 0~4095
 * gVoltageMv: 根据 3.3 V 参考电压换算出的毫伏值
 */
volatile uint16_t gAdcValue;
volatile uint32_t gVoltageMv;

/* SysTick 每 1 ms 累加一次，用于控制 ADC 采样和打印周期。 */
volatile uint32_t gMsTicks;

void SysTick_Handler(void)
{
    gMsTicks++;
}

int main(void)
{
    uint32_t lastPrintTime = 0U;

    SYSCFG_DL_init();

    DL_SYSTICK_init(CPUCLK_FREQ / 1000U);
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();

    HC05_SendString(HC05_UART_INST,
        "\r\nMSPM0 ADC monitor ready.\r\n");

    while (1) {
        uint32_t now = gMsTicks;

        if ((uint32_t) (now - lastPrintTime) < ADC_PRINT_INTERVAL_MS) {
            __WFI();
            continue;
        }
        lastPrintTime = now;

        gAdcValue = ADC_ReadRaw(ADC12_0_INST);
        gVoltageMv = ADC_RawToMillivolts(gAdcValue, 3300U);

        HC05_SendString(HC05_UART_INST, "ADC=");
        HC05_SendUint32(HC05_UART_INST, gAdcValue);
        HC05_SendString(HC05_UART_INST, ", Voltage=");
        HC05_SendUint32(HC05_UART_INST, gVoltageMv);
        HC05_SendString(HC05_UART_INST, " mV\r\n");
    }
}
