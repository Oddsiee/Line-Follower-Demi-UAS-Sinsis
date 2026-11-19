#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include "config.h"
#include "params.h"

/* ============================================================
 * calibration.h — Kalibrasi min/max & normalisasi 16CH (M5)
 * ============================================================ */

void    Calib_Start(void);
void    Calib_Update(const uint16_t raw[SENSOR_COUNT]);
void    Calib_Finish(Params_t *p);
void    Calib_Normalize(const Params_t *p,
                        const uint16_t raw[SENSOR_COUNT],
                        uint16_t norm[SENSOR_COUNT]);
void    Calib_GetLiveMinMax(uint16_t out_min[SENSOR_COUNT],
                            uint16_t out_max[SENSOR_COUNT],
                            uint32_t *samples);
uint8_t Calib_CountReadyChannels(uint16_t min_span);

#endif /* CALIBRATION_H */
