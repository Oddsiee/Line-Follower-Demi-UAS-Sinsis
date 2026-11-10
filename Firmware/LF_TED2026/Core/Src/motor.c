#include "motor.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * motor.c — driver ZK-5AD dual H-bridge via TIM1 PWM
 *
 * Pin (lihat config.h):
 *   D0 = PA8  (TIM1_CH1)  -> Motor kiri  IN1
 *   D1 = PA9  (TIM1_CH2)  -> Motor kiri  IN2
 *   D2 = PA10 (TIM1_CH3)  -> Motor kanan IN1
 *   D3 = PA11 (TIM1_CH4)  -> Motor kanan IN2
 *
 * Logika (sesuai dokumentasi master):
 *   Maju   : IN1 = PWM, IN2 = LOW
 *   Mundur : IN1 = LOW, IN2 = PWM
 *   Rem    : IN1 = HIGH, IN2 = HIGH
 *   Coast  : IN1 = LOW,  IN2 = LOW
 * ============================================================ */

extern TIM_HandleTypeDef htim1;

static void motor_apply(uint32_t ch_fwd, uint32_t ch_rev, int16_t cmd)
{
	if (cmd > MOTOR_CMD_MAX)  cmd = MOTOR_CMD_MAX;
	if (cmd < -MOTOR_CMD_MAX) cmd = -MOTOR_CMD_MAX;

	if (cmd > 0) {
		__HAL_TIM_SET_COMPARE(&htim1, ch_fwd, (uint32_t)cmd);
		__HAL_TIM_SET_COMPARE(&htim1, ch_rev, 0U);
	} else if (cmd < 0) {
		__HAL_TIM_SET_COMPARE(&htim1, ch_fwd, 0U);
		__HAL_TIM_SET_COMPARE(&htim1, ch_rev, (uint32_t)(-cmd));
	} else {
		__HAL_TIM_SET_COMPARE(&htim1, ch_fwd, 0U);
		__HAL_TIM_SET_COMPARE(&htim1, ch_rev, 0U);
	}
}

void Motor_Init(void)
{
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
	Motor_Coast();
}

void Motor_Set(int16_t left, int16_t right)
{
	motor_apply(TIM_CHANNEL_1, TIM_CHANNEL_2, left);
	motor_apply(TIM_CHANNEL_3, TIM_CHANNEL_4, right);
}

void Motor_Brake(void)
{
	/* Kedua pin HIGH = rem aktif. ARR = MOTOR_PWM_ARR -> 100% duty. */
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, MOTOR_PWM_ARR);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, MOTOR_PWM_ARR);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, MOTOR_PWM_ARR);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, MOTOR_PWM_ARR);
}

void Motor_Coast(void)
{
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0U);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0U);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0U);
}
