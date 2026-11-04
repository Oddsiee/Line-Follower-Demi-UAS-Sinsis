#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>
#include "config.h"

/* Struct parameter bersama: diedit UI, disimpan Storage, dibaca PID/Track. */
typedef struct {
    uint32_t magic;                       /* PARAMS_MAGIC                   */
    float    kp, ki, kd;
    int16_t  base_speed;                  /* 0..MOTOR_CMD_MAX               */
    int16_t  max_speed;                   /* 0..MOTOR_CMD_MAX               */
    uint16_t cal_min[SENSOR_COUNT];       /* nilai ADC min (putih/hitam)    */
    uint16_t cal_max[SENSOR_COUNT];       /* nilai ADC max                  */
    uint8_t  invert;                      /* 1 = garis putih di atas hitam  */
    uint8_t  reserved[3];
    uint32_t checksum;                    /* diisi Storage_Save             */
} Params_t;

void Params_SetDefaults(Params_t *p);

#endif /* PARAMS_H */
