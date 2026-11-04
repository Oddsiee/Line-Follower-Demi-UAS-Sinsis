#ifndef TRACK_H
#define TRACK_H

#include <stdint.h>
#include "config.h"
#include "params.h"

/* Pemilik: C (A mendukung) */
typedef enum {
    TRACK_EVT_NONE = 0,
    TRACK_EVT_JUNCTION,
    TRACK_EVT_CHECKPOINT,
    TRACK_EVT_INVERT_ON,
    TRACK_EVT_INVERT_OFF
} TrackEvent_t;

void         Track_Init(void);
/* norm = hasil Calib_Normalize. Kembalikan event terbaru (atau NONE). */
TrackEvent_t Track_Update(const uint16_t norm[SENSOR_COUNT], Params_t *p);
uint8_t      Track_GetCheckpointIndex(void);   /* 0..4 */

#endif /* TRACK_H */
