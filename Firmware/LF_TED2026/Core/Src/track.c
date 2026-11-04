#include "track.h"

/* TODO (M9, pemilik C): deteksi persimpangan (banyak sensor aktif),
 * indexing CP 1-4, dan toggle invert untuk blok "TED 2026"/petir. */

static uint8_t s_cp = 0;

void Track_Init(void) { s_cp = 0; }

TrackEvent_t Track_Update(const uint16_t norm[SENSOR_COUNT], Params_t *p)
{
    (void)norm; (void)p;
    return TRACK_EVT_NONE;                                   /* stub */
}

uint8_t Track_GetCheckpointIndex(void) { return s_cp; }
