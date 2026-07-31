#include <errno.h>
#include <unistd.h>

#include "hc05.h"
#include "ti_msp_dl_config.h"

/*
 * newlib-nano ultimately routes printf()/puts() output through _write().
 * The UART must already have been initialized by SYSCFG_DL_init().
 */
int _write(int fileDescriptor, const char *data, int length)
{
    int index;

    if ((fileDescriptor != STDOUT_FILENO) &&
        (fileDescriptor != STDERR_FILENO)) {
        errno = EBADF;
        return -1;
    }

    if ((data == NULL) || (length < 0)) {
        errno = EINVAL;
        return -1;
    }

    for (index = 0; index < length; index++) {
        if (data[index] == '\n') {
            HC05_SendByte(HC05_UART_INST, (uint8_t) '\r');
        }
        HC05_SendByte(HC05_UART_INST, (uint8_t) data[index]);
    }

    return length;
}
