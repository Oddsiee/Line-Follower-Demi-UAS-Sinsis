#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/* ============================================================
 * sensor.h — Driver Sensor 16CH Dariyan-X (Dual 4-Ch Mux + ADC1 DMA)
 *
 * Pin (config.h):
 *   Out1..Out4 = PA0/PA1/PA2/PB1 (ADC1_IN0, IN1, IN2, IN9)
 *   In1 = PA3 (mux bit0), In2 = PB0 (mux bit1)
 * ============================================================ */

void     Sensor_Init(void);
void     Sensor_ReadRaw(uint16_t raw[SENSOR_COUNT]);
int16_t  Sensor_ComputePosition(const uint16_t norm[SENSOR_COUNT], bool *line_visible);
int16_t  Sensor_GetPosition(void);
bool     Sensor_LineVisible(void);
uint32_t Sensor_GetLastScanUs(void);
uint32_t Sensor_GetAdcErr(void);

extern volatile uint32_t g_sensor_scan_us;
extern volatile uint32_t g_sensor_adc_err;

#endif /* SENSOR_H */
