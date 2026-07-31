#include "hc05.h"

#include <stddef.h>

#define HC05_RX_BUFFER_SIZE (64U)

/*
 * head 只由 UART 中断修改，tail 只由主循环修改。
 * 循环队列保留一个空位置用于区分“空”和“满”，因此最多保存 63 字节。
 */
static volatile uint8_t gRxBuffer[HC05_RX_BUFFER_SIZE];
static volatile uint16_t gRxHead;
static volatile uint16_t gRxTail;
static volatile uint32_t gRxByteCount;
static volatile bool gRxOverflow;

void HC05_SendByte(UART_Regs *uart, uint8_t data)
{
    DL_UART_Main_transmitDataBlocking(uart, data);
}

void HC05_SendBuffer(
    UART_Regs *uart, const uint8_t *data, uint32_t length)
{
    uint32_t index;

    if (data == NULL) {
        return;
    }

    for (index = 0U; index < length; index++) {
        HC05_SendByte(uart, data[index]);
    }
}

void HC05_SendString(UART_Regs *uart, const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        HC05_SendByte(uart, (uint8_t) *text);
        text++;
    }
}

void HC05_SendUint32(UART_Regs *uart, uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    if (value == 0U) {
        HC05_SendByte(uart, (uint8_t) '0');
        return;
    }

    while (value > 0U) {
        digits[count] = (char) ('0' + (value % 10U));
        count++;
        value /= 10U;
    }

    while (count > 0U) {
        count--;
        HC05_SendByte(uart, (uint8_t) digits[count]);
    }
}

void HC05_SendInt32(UART_Regs *uart, int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        HC05_SendByte(uart, (uint8_t) '-');
        magnitude = (uint32_t) (-(int64_t) value);
    } else {
        magnitude = (uint32_t) value;
    }

    HC05_SendUint32(uart, magnitude);
}

void HC05_SendHex8(UART_Regs *uart, uint8_t value)
{
    static const char hexDigits[] = "0123456789ABCDEF";

    HC05_SendByte(
        uart, (uint8_t) hexDigits[(value >> 4U) & 0x0FU]);
    HC05_SendByte(
        uart, (uint8_t) hexDigits[value & 0x0FU]);
}

void HC05_ResetReceiver(void)
{
    gRxHead = 0U;
    gRxTail = 0U;
    gRxByteCount = 0U;
    gRxOverflow = false;
}

void HC05_HandleRxInterrupt(UART_Regs *uart)
{
    uint16_t nextHead;
    uint8_t data;

    if (uart == NULL) {
        return;
    }

    /*
     * 读取 IIDX 会确认当前中断。随后无条件排空 RX FIFO，避免其他 UART
     * pending 状态使已经到达的字节滞留在硬件 FIFO 中。
     */
    (void) DL_UART_Main_getPendingInterrupt(uart);

    while (DL_UART_Main_isRXFIFOEmpty(uart) != true) {
        data = DL_UART_Main_receiveData(uart);
        if (gRxByteCount != UINT32_MAX) {
            gRxByteCount++;
        }
        nextHead = (uint16_t) ((gRxHead + 1U) %
            HC05_RX_BUFFER_SIZE);

        if (nextHead == gRxTail) {
            gRxOverflow = true;
        } else {
            gRxBuffer[gRxHead] = data;
            gRxHead = nextHead;
        }
    }
}

bool HC05_DataAvailable(void)
{
    return (gRxHead != gRxTail);
}

bool HC05_ReadByte(uint8_t *data)
{
    if ((data == NULL) || (HC05_DataAvailable() == false)) {
        return false;
    }

    *data = gRxBuffer[gRxTail];
    gRxTail = (uint16_t) ((gRxTail + 1U) % HC05_RX_BUFFER_SIZE);

    return true;
}

uint32_t HC05_GetRxByteCount(void)
{
    return gRxByteCount;
}

bool HC05_RxOverflowed(void)
{
    return gRxOverflow;
}

void HC05_ClearRxOverflow(void)
{
    gRxOverflow = false;
}
