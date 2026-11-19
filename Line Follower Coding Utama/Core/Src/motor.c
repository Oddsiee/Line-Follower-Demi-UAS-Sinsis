#include "motor.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * motor.c — Driver ZK-5AD dual H-bridge via TIM1 PWM
 *
 * Pemetaan kanal fisik robot:
 *   D0 = PA8  (TIM1_CH1) -> Motor kanan IN2 (Mundur)
 *   D1 = PA9  (TIM1_CH2) -> Motor kanan IN1 (Maju)
 *   D2 = PA10 (TIM1_CH3) -> Motor kiri  IN2 (Mundur)
 *   D3 = PA11 (TIM1_CH4) -> Motor kiri  IN1 (Maju)
 * ============================================================ */

extern TIM_HandleTypeDef htim1;

static uint32_t s_freq_hz    = 1000U;
static bool     s_slow_decay = false;

static uint32_t motor_get_tim1_clk(void)
{
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1) {
        pclk2 *= 2U;
    }
    return pclk2;
}

static void motor_apply(uint32_t ch_fwd, uint32_t ch_rev, int16_t cmd)
{
    if (cmd > MOTOR_CMD_MAX) {
        cmd = MOTOR_CMD_MAX;
    }
    if (cmd < -MOTOR_CMD_MAX) {
        cmd = -MOTOR_CMD_MAX;
    }

    if (cmd > 0) {
        uint32_t mag = (uint32_t)cmd;
        if (s_slow_decay) {
            __HAL_TIM_SET_COMPARE(&htim1, ch_fwd, (uint32_t)MOTOR_CMD_MAX);
            __HAL_TIM_SET_COMPARE(&htim1, ch_rev, (uint32_t)MOTOR_CMD_MAX - mag);
        } else {
            __HAL_TIM_SET_COMPARE(&htim1, ch_fwd, mag);
            __HAL_TIM_SET_COMPARE(&htim1, ch_rev, 0U);
        }
    } else if (cmd < 0) {
        uint32_t mag = (uint32_t)(-cmd);
        if (s_slow_decay) {
            __HAL_TIM_SET_COMPARE(&htim1, ch_fwd, (uint32_t)MOTOR_CMD_MAX - mag);
            __HAL_TIM_SET_COMPARE(&htim1, ch_rev, (uint32_t)MOTOR_CMD_MAX);
        } else {
            __HAL_TIM_SET_COMPARE(&htim1, ch_fwd, 0U);
            __HAL_TIM_SET_COMPARE(&htim1, ch_rev, mag);
        }
    } else {
        __HAL_TIM_SET_COMPARE(&htim1, ch_fwd, 0U);
        __HAL_TIM_SET_COMPARE(&htim1, ch_rev, 0U);
    }
}

void Motor_Init(void)
{
    s_slow_decay = false;
    Motor_SetFreqHz(1000U);

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    __HAL_TIM_MOE_ENABLE(&htim1);
    Motor_Coast();
}

void Motor_Set(int16_t left, int16_t right)
{
    motor_apply(TIM_CHANNEL_4, TIM_CHANNEL_3, left);   /* Kiri  : Maju=CH4(PA11), Mundur=CH3(PA10) */
    motor_apply(TIM_CHANNEL_2, TIM_CHANNEL_1, right);  /* Kanan : Maju=CH2(PA9),  Mundur=CH1(PA8)  */
}

void Motor_Brake(void)
{
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

void Motor_SetChannelRaw(uint8_t ch_idx, uint16_t ccr)
{
    static const uint32_t CHANNELS[4] = {
        TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
    };
    if (ccr > (uint16_t)MOTOR_CMD_MAX) {
        ccr = (uint16_t)MOTOR_CMD_MAX;
    }
    for (uint8_t i = 0U; i < 4U; i++) {
        __HAL_TIM_SET_COMPARE(&htim1, CHANNELS[i], (i == ch_idx) ? (uint32_t)ccr : 0U);
    }
}

void Motor_SetFreqHz(uint32_t freq_hz)
{
    if (freq_hz < 100U) {
        freq_hz = 100U;
    }
    if (freq_hz > 50000U) {
        freq_hz = 50000U;
    }

    uint32_t tim_clk = motor_get_tim1_clk();
    uint32_t period  = (uint32_t)MOTOR_PWM_ARR + 1U;
    uint32_t denom   = freq_hz * period;
    uint32_t div     = (tim_clk + (denom / 2U)) / denom;
    if (div < 1U) {
        div = 1U;
    }
    if (div > 65536U) {
        div = 65536U;
    }

    uint32_t psc = div - 1U;
    htim1.Init.Prescaler = psc;
    __HAL_TIM_SET_PRESCALER(&htim1, psc);
    htim1.Instance->EGR = TIM_EGR_UG;

    s_freq_hz = tim_clk / (div * period);
}

uint32_t Motor_GetFreqHz(void)
{
    return s_freq_hz;
}

void Motor_SetSlowDecay(bool enable)
{
    s_slow_decay = enable;
}

bool Motor_GetSlowDecay(void)
{
    return s_slow_decay;
}
