#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* Pemilik: A */
void    Sensor_Init(void);

/* Scan mux 4 state x 4 channel ADC -> raw[16]. Indeks = group*4 + mux_state.
 * Urutan fisik channel diverifikasi di Milestone 2. */
void    Sensor_ReadRaw(uint16_t raw[SENSOR_COUNT]);

/* Posisi garis -SENSOR_POS_MAX..+SENSOR_POS_MAX (0 = tengah). Pakai kalibrasi. */
int16_t Sensor_GetPosition(void);

/* false jika tidak ada sensor yang melihat garis. */
bool    Sensor_LineVisible(void);

#endif /* SENSOR_H */
