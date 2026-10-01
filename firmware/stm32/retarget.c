/*
 * retarget.c - send printf output out of USART2.
 *
 * printf() in newlib formats the text, then calls _write() to actually
 * output the bytes. CubeIDE's syscalls.c defines a weak _write that does
 * nothing useful, so defining our own here replaces it at link time.
 *
 * On the Nucleo, USART2 is wired to the ST-LINK chip, which shows up on
 * the PC as a USB serial port (the "virtual COM port").
 *
 * Copy this file into Core/Src/ of the generated CubeIDE project.
 */
#include "main.h"

extern UART_HandleTypeDef huart2; /* created by CubeMX in main.c */

int _write(int file, char *ptr, int len)
{
    (void)file; /* stdout and stderr both go to the UART */

    /* Blocking send: waits until every byte is out. Fine for bring-up,
     * but at 115200 baud each character takes about 87 us, so this must
     * stay out of anything timing-critical later (like the 200 Hz sampling). */
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}
