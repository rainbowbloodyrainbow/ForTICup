#include "ti_msp_dl_config.h"

#include "hc05.h"

/* 最近一次从 HC-05 收到的字节，可在调试器中观察。 */
volatile uint8_t gReceivedByte;

int main(void)
{
    uint8_t receivedByte;

    SYSCFG_DL_init();
    HC05_ResetReceiver();

    /* 发送上电提示。连接蓝牙串口后复位开发板即可看到。 */
    HC05_SendString(
        HC05_UART_INST,
        "\r\nMSPM0 HC-05 ready. Echo mode.\r\n");

    /* SysConfig 已打开 UART 的 RX 中断，这里再打开 CPU 侧的 UART1 中断。 */
    NVIC_ClearPendingIRQ(HC05_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(HC05_UART_INST_INT_IRQN);

    while (1) {
        /*
         * 中断只负责把数据放入循环队列；回显在主循环完成，
         * 避免在中断函数中执行阻塞式字符串发送。
         */
        if (HC05_ReadByte(&receivedByte)) {
            gReceivedByte = receivedByte;
            HC05_SendByte(HC05_UART_INST, receivedByte);
        }
    }
}

void HC05_UART_INST_IRQHandler(void)
{
    HC05_HandleRxInterrupt(HC05_UART_INST);
}
