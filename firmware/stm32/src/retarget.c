/*
 * retarget.c - send printf output out of USART2.
 *
 * printf() in newlib formats the text, then calls _write() to actually
 * output the bytes. The C library only has a do-nothing _write stub, so
 * defining our own here replaces it when the program is linked.
 *
 * On the Nucleo, USART2 goes to the ST-LINK chip, which shows up on the PC
 * as a USB serial port.
 *
 * Only one task prints for now. If several tasks print at once their text
 * can interleave; a later step adds a mutex or a dedicated print task.
 */
#include "board.h"

int _write(int file, char *ptr, int len);

int _write(int file, char *ptr, int len)
{
    (void)file; /* stdout and stderr both go to the UART */

    /* Blocking send: waits until every byte is out. Fine for bring-up,
     * but at 115200 baud each character takes about 87 us, so this must
     * stay out of anything timing-critical later (like the 200 Hz
     * sampling). */
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}
