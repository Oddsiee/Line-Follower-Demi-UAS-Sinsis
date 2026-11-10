#include "debug.h"
#include "stm32f4xx_hal.h"
#include <stdarg.h>
#include <stdio.h>

/* ============================================================
 * debug.c — USART1 115200 8N1
 *   TX = PA15 (AF7), RX = PB3 (AF7)
 * Format baris untuk Tools/uart_plot.py:
 *   S,v0,v1,...,v15,pos\r\n
 * ============================================================ */

extern UART_HandleTypeDef huart1;

void Debug_Init(void)
{
    /* USART1 + GPIO sudah diinisialisasi CubeMX (MX_USART1_UART_Init). */
}

void Debug_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 20);
    }
}

void Debug_SendSensors(const uint16_t raw[SENSOR_COUNT], int16_t pos)
{
    Debug_Printf("S,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
                 raw[0],  raw[1],  raw[2],  raw[3],
                 raw[4],  raw[5],  raw[6],  raw[7],
                 raw[8],  raw[9],  raw[10], raw[11],
                 raw[12], raw[13], raw[14], raw[15],
                 (int)pos);
}
