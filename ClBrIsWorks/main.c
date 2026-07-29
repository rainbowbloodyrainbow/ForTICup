#include "application.h"
#include "hc05.h"
#include "system_time.h"
#include "ti_msp_dl_config.h"

int main(void)
{
    SYSCFG_DL_init();
    Application_Init();

    while (1) {
        Application_Process();
        __WFI();
    }
}

void SysTick_Handler(void)
{
    SystemTime_On1msTick();
}

void HC05_UART_INST_IRQHandler(void)
{
    HC05_HandleRxInterrupt(HC05_UART_INST);
}
