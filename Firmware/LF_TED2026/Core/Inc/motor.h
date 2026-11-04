#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include "config.h"

/* Pemilik: B
 * Maju   : pin 1 (D0/D2) = PWM, pin 2 (D1/D3) = LOW
 * Mundur : pin 1 = LOW,        pin 2 = PWM
 * Rem    : kedua pin HIGH
 * Coast  : kedua pin LOW
 * Pin 3V3 dan VT di header ZK-5AD tidak disambung. */
void Motor_Init(void);
void Motor_Set(int16_t left, int16_t right);   /* -1000..+1000 */
void Motor_Brake(void);
void Motor_Coast(void);

#endif /* MOTOR_H */
