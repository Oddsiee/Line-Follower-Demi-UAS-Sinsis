#include "calibration.h"

/* TODO (M5, pemilik A): simpan min/max tiap channel selama sapu,
 * lalu normalisasi ke 0..SENSOR_NORM_MAX. */

void Calib_Start(void) { }
void Calib_Update(const uint16_t raw[SENSOR_COUNT]) { (void)raw; }
void Calib_Finish(Params_t *p) { (void)p; }

void Calib_Normalize(const Params_t *p,
                     const uint16_t raw[SENSOR_COUNT],
                     uint16_t norm[SENSOR_COUNT])
{
    (void)p;
    for (int i = 0; i < SENSOR_COUNT; i++) norm[i] = raw[i];   /* stub */
}
