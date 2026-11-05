/**
 * hold_test.c — Tes kontinuitas kabel SCL/SDA (pakai multimeter)
 *
 * Firmware ini membuat PB6 (SCL) dan PB7 (SDA) bergantian:
 *   - 2 detik HIGH (3.3 V)
 *   - 2 detik LOW  (0 V)
 * berulang terus.
 *
 * CARA PAKAI:
 *   1. Flash firmware ini.
 *   2. Set multimeter ke DC volt, probe hitam ke GND board.
 *   3. Probe merah tempel ke pin SCL di MODUL OLED (bukan di board).
 *   4. Perhatikan angkanya selama ~10 detik.
 *
 * HASIL:
 *   - Angka naik-turun 3.3V <-> 0V  => KABEL OK, modul yang bermasalah
 *   - Angka tetap 0V / 0.03V         => KABEL PUTUS / tidak kontak
 *   - Angka tetap 3.3V               => short ke 3V3 (jarang)
 *
 * Ulangi untuk pin SDA.
 *
 * LED PC13 berkedip cepat = firmware jalan.
 */

#include "stm32f4xx_hal.h"

volatile uint32_t g_phase = 0;   /* 0 = HIGH, 1 = LOW */
volatile uint32_t g_loop  = 0;

void Error_Handler(void) { __disable_irq(); for (;;) { } }

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC13 LED */
    GPIO_InitTypeDef led = {0};
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    led.Pin = GPIO_PIN_13; led.Mode = GPIO_MODE_OUTPUT_PP;
    led.Pull = GPIO_NOPULL; led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &led);

    /* PB6 & PB7: push-pull output (bukan open-drain) supaya bisa drive penuh */
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    for (;;)
    {
        g_loop++;

        /* 2 detik HIGH */
        g_phase = 0;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
        for (int i = 0; i < 20; i++) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(100);
        }

        /* 2 detik LOW */
        g_phase = 1;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
        for (int i = 0; i < 20; i++) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(100);
        }
    }
}
