#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/* ============================================================
 * motor.h — Driver motor ZK-5AD dual H-bridge (TIM1 CH1..CH4)
 *
 * Pemetaan arah aktual robot (terverifikasi fisik):
 *   Motor Kiri  (left)  : Maju = PA11 (D3/CH4), Mundur = PA10 (D2/CH3)
 *   Motor Kanan (right) : Maju = PA9  (D1/CH2), Mundur = PA8  (D0/CH1)
 *
 * Logika H-bridge (ZK-5AD):
 *   Maju   : ch_fwd = PWM,  ch_rev = LOW   (atau HIGH / invPWM di Slow Decay)
 *   Mundur : ch_fwd = LOW,  ch_rev = PWM
 *   Rem    : kedua pin HIGH
 *   Coast  : kedua pin LOW
 * Pin 3V3 dan VT pada header ZK-5AD tidak disambung (NC).
 * ============================================================ */

void     Motor_Init(void);
void     Motor_Set(int16_t left, int16_t right);          /* -1000..+1000 */
void     Motor_Brake(void);
void     Motor_Coast(void);
void     Motor_SetChannelRaw(uint8_t ch_idx, uint16_t ccr); /* ch_idx 0..3 = D0..D3 */
void     Motor_SetFreqHz(uint32_t freq_hz);
uint32_t Motor_GetFreqHz(void);
void     Motor_SetSlowDecay(bool enable);
bool     Motor_GetSlowDecay(void);

#endif /* MOTOR_H */
