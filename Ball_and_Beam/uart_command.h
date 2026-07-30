#ifndef UART_COMMAND_H
#define UART_COMMAND_H

#include <stdint.h>

void uart_command_init(void);
void uart_command_handle_rx_interrupt(void);
void uart_command_process(void);
void uart_command_send_string(const char *text);

#endif
