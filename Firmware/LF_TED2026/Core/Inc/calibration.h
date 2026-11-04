#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include "config.h"
#include "params.h"

/* Pemilik: A */
void     Calib_Start(void);                         /* mulai sapu hitam/putih */
void     Calib_Update(const uint16_t raw[SENSOR_COUNT]);
void     Calib_Finish(Params_t *p);                 /* tulis cal_min/cal_max  */

/* raw -> 0..SENSOR_NORM_MAX (mempertimbangkan p->invert). */
void     Calib_Normalize(const Params_t *p,
                         const uint16_t raw[SENSOR_COUNT],
                         uint16_t norm[SENSOR_COUNT]);

#endif /* CALIBRATION_H */
