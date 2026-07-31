#ifndef HC05_H
#define HC05_H

#include <stdbool.h>
#include <stdint.h>

#include <ti/driverlib/dl_uart_main.h>

/*
 * HC-05 正常工作模式是透明串口，因此 UART 的实例、波特率和引脚
 * 应先在应用工程的 SysConfig 中配置，并由 SYSCFG_DL_init() 初始化。
 */
void HC05_SendByte(UART_Regs *uart, uint8_t data);
void HC05_SendBuffer(
    UART_Regs *uart, const uint8_t *data, uint32_t length);
void HC05_SendString(UART_Regs *uart, const char *text);

void HC05_SendUint32(UART_Regs *uart, uint32_t value);
void HC05_SendInt32(UART_Regs *uart, int32_t value);
void HC05_SendHex8(UART_Regs *uart, uint8_t value);

/*
 * 接收采用一个模块内部的循环队列。
 *
 * ResetReceiver() 在打开 CPU 侧 UART 中断前调用。
 * HandleRxInterrupt() 由应用工程自己的 UART IRQHandler 调用。
 * 主循环通过 DataAvailable() 和 ReadByte() 读取数据，不要再同时使用
 * DL_UART_Main_receiveDataBlocking()，否则两处代码会争抢同一份接收数据。
 */
void HC05_ResetReceiver(void);
void HC05_HandleRxInterrupt(UART_Regs *uart);
bool HC05_DataAvailable(void);
bool HC05_ReadByte(uint8_t *data);
uint32_t HC05_GetRxByteCount(void);
bool HC05_RxOverflowed(void);
void HC05_ClearRxOverflow(void);

#endif
