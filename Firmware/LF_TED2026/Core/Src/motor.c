#include "motor.h"

/* TODO (M3, pemilik B): isi dengan HAL TIM1 PWM (htim1, CH1..CH4).
 * Gunakan __HAL_TIM_SET_COMPARE dan MOTOR_PWM_ARR.
 * Kiri  = PA8 (D0), PA9 (D1);  Kanan = PA10 (D2), PA11 (D3) atau pin pengganti. */

void Motor_Init(void) { }
void Motor_Set(int16_t left, int16_t right) { (void)left; (void)right; }
void Motor_Brake(void) { }
void Motor_Coast(void) { }
