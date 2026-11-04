#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include "config.h"

/* UART debug (USART1, 115200 baud). Format baris untuk Tools/uart_plot.py:
 *   S,v0,v1,...,v15,pos\n
 */
void Debug_Init(void);
void Debug_Printf(const char *fmt, ...);
void Debug_SendSensors(const uint16_t raw[SENSOR_COUNT], int16_t pos);

#endif /* DEBUG_H */
