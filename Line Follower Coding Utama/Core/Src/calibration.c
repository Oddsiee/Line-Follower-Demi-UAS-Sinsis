#include "calibration.h"
#include <stddef.h>

static uint16_t s_min[SENSOR_COUNT];
static uint16_t s_max[SENSOR_COUNT];
static uint32_t s_samples = 0U;

void Calib_Start(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        s_min[i] = (uint16_t)ADC_MAX;
        s_max[i] = 0U;
    }
    s_samples = 0U;
}

void Calib_Update(const uint16_t raw[SENSOR_COUNT])
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (raw[i] < s_min[i]) {
            s_min[i] = raw[i];
        }
        if (raw[i] > s_max[i]) {
            s_max[i] = raw[i];
        }
    }
    s_samples++;
}

void Calib_Finish(Params_t *p)
{
    if (p == NULL) {
        return;
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if ((s_samples > 0U) && (s_max[i] > (uint16_t)(s_min[i] + 50U))) {
            p->cal_min[i] = s_min[i];
            p->cal_max[i] = s_max[i];
        } else {
            /* Fallback aman agar tidak terjadi pembagian nol */
            p->cal_min[i] = 0U;
            p->cal_max[i] = (uint16_t)ADC_MAX;
        }
    }
}

void Calib_Normalize(const Params_t *p,
                     const uint16_t raw[SENSOR_COUNT],
                     uint16_t norm[SENSOR_COUNT])
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        uint16_t lo = p->cal_min[i];
        uint16_t hi = p->cal_max[i];
        uint16_t val;

        if (hi <= (uint16_t)(lo + 10U)) {
            val = (uint16_t)(((uint32_t)raw[i] * (uint32_t)SENSOR_NORM_MAX) / (uint32_t)ADC_MAX);
        } else if (raw[i] <= lo) {
            val = 0U;
        } else if (raw[i] >= hi) {
            val = (uint16_t)SENSOR_NORM_MAX;
        } else {
            val = (uint16_t)(((uint32_t)(raw[i] - lo) * (uint32_t)SENSOR_NORM_MAX) / (uint32_t)(hi - lo));
        }

        if (p->invert != 0U) {
            val = (uint16_t)((uint16_t)SENSOR_NORM_MAX - val);
        }

        norm[i] = val;
    }
}

void Calib_GetLiveMinMax(uint16_t out_min[SENSOR_COUNT],
                         uint16_t out_max[SENSOR_COUNT],
                         uint32_t *samples)
{
    if (out_min != NULL) {
        for (int i = 0; i < SENSOR_COUNT; i++) {
            out_min[i] = s_min[i];
        }
    }
    if (out_max != NULL) {
        for (int i = 0; i < SENSOR_COUNT; i++) {
            out_max[i] = s_max[i];
        }
    }
    if (samples != NULL) {
        *samples = s_samples;
    }
}

uint8_t Calib_CountReadyChannels(uint16_t min_span)
{
    uint8_t ok = 0U;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if ((s_max[i] > s_min[i]) && ((uint16_t)(s_max[i] - s_min[i]) >= min_span)) {
            ok++;
        }
    }
    return ok;
}
