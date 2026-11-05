/**
 * pullup_test.c — Deteksi apakah modul OLED benar-benar terhubung & berdaya
 *
 * Logika:
 *   Modul OLED SH1106 normal punya resistor pull-up 4.7k-10k di SCL & SDA.
 *   Pull-down internal MCU ~40k (lemah).
 *
 *   - Kalau modul TERHUBUNG & BERDAYA:
 *       pull-up 10k mengalahkan pull-down 40k  ->  line terbaca HIGH
 *   - Kalau modul TIDAK terhubung / tidak berdaya:
 *       pull-down 40k menarik line ke GND      ->  line terbaca LOW
 *
 * Jadi tes ini membedakan "modul mati" vs "modul tidak tersambung".
 *
 * Hasil di g_* dibaca via SWD.
 */

#include "stm32f4xx_hal.h"

volatile uint32_t g_pd_pb6   = 0;  /* PB6 dengan pull-down internal */
volatile uint32_t g_pd_pb7   = 0;  /* PB7 dengan pull-down internal */
volatile uint32_t g_pd_both  = 0;  /* keduanya, bit6 & bit7 */
volatile uint32_t g_pu_both  = 0;  /* keduanya dengan pull-up (pembanding) */
volatile uint32_t g_float    = 0;  /* tanpa pull sama sekali */
volatile uint32_t g_done     = 0;
volatile uint32_t g_loop     = 0;

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

static void set_pull(uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = pull;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
}

static uint32_t idr(void) { return (GPIOB->IDR & (GPIO_PIN_6 | GPIO_PIN_7)); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef led = {0};
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    led.Pin = GPIO_PIN_13; led.Mode = GPIO_MODE_OUTPUT_PP;
    led.Pull = GPIO_NOPULL; led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &led);

    /* --- PB6 saja, pull-down --- */
    {
        GPIO_InitTypeDef g = {0};
        g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLDOWN; g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_7; g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &g);
        HAL_Delay(30);
        g_pd_pb6 = idr();
    }

    /* --- PB7 saja, pull-down --- */
    {
        GPIO_InitTypeDef g = {0};
        g.Pin = GPIO_PIN_7; g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLDOWN; g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_6; g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &g);
        HAL_Delay(30);
        g_pd_pb7 = idr();
    }

    /* --- keduanya pull-down --- */
    set_pull(GPIO_PULLDOWN);
    HAL_Delay(30);
    g_pd_both = idr();

    /* --- keduanya pull-up (pembanding) --- */
    set_pull(GPIO_PULLUP);
    HAL_Delay(30);
    g_pu_both = idr();

    /* --- keduanya float --- */
    set_pull(GPIO_NOPULL);
    HAL_Delay(30);
    g_float = idr();

    g_done = 1;

    for (;;) {
        g_loop++;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(950);
    }
}
