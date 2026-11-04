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
    for (int i = 0; i < SENSOR_COUNT; i++) {
        p->cal_min[i] = 0;
        p->cal_max[i] = ADC_MAX;
    }
}
