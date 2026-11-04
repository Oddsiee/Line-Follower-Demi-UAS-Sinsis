#include "debug.h"

/* TODO (M1, bersama): USART1 115200 baud (PA15 TX / PB3 RX).
 * Debug_SendSensors mencetak: S,v0,...,v15,pos\n */

void Debug_Init(void) { }
void Debug_Printf(const char *fmt, ...) { (void)fmt; }
void Debug_SendSensors(const uint16_t raw[SENSOR_COUNT], int16_t pos)
{
    (void)raw; (void)pos;
}
