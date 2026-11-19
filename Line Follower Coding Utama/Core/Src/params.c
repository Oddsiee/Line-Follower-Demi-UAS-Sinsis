#include "params.h"
#include <string.h>

void Params_SetDefaults(Params_t *p)
{
    memset(p, 0, sizeof(*p));
    p->magic      = PARAMS_MAGIC;
    p->kp         = DEFAULT_KP;
    p->ki         = DEFAULT_KI;
    p->kd         = DEFAULT_KD;
    p->base_speed = DEFAULT_BASE_SPEED;
    p->max_speed  = DEFAULT_MAX_SPEED;
    p->invert     = 0U;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        p->cal_min[i] = 0U;
        p->cal_max[i] = (uint16_t)ADC_MAX;
    }
}
