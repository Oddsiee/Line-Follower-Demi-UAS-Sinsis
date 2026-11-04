#include "sensor.h"

/* TODO (M2, pemilik A):
 *  - set MUX_IN1/MUX_IN2 untuk state 0..3, tunggu MUX_SETTLE_US
 *  - baca PA4..PA7 (ADC1 via DMA), isi raw[group * MUX_STATES + state]
 *  - verifikasi urutan fisik channel dengan sensor di atas garis
 * TODO (M5): Sensor_GetPosition (weighted average), Sensor_LineVisible */

void Sensor_Init(void) { }

void Sensor_ReadRaw(uint16_t raw[SENSOR_COUNT])
{
    for (int i = 0; i < SENSOR_COUNT; i++) raw[i] = 0;   /* stub */
}

int16_t Sensor_GetPosition(void) { return 0; }           /* stub */

bool Sensor_LineVisible(void) { return false; }          /* stub */
