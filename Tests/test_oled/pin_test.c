/**
 * pin_test.c — Uji fisik pin I2C + scan semua alamat I2C
 *
 * Menjawab: apakah SCL/SDA ditahan LOW oleh sesuatu di luar MCU?
 *
 * Hasil disimpan di variabel g_* dan dibaca via SWD (lihat probe_pins.ps1).
 */

#include "stm32f4xx_hal.h"

I2C_HandleTypeDef hi2c1;

volatile uint32_t g_t1_pullup   = 0;  /* input + pull-up internal  */
volatile uint32_t g_t2_nopull   = 0;  /* input + no pull           */
volatile uint32_t g_t3_drive_hi = 0;  /* output push-pull HIGH     */
volatile uint32_t g_t4_drive_lo = 0;  /* output push-pull LOW      */
volatile uint32_t g_scan_lo     = 0;  /* alamat 0x08..0x3F (bit i = 0x08+i) */
volatile uint32_t g_scan_hi     = 0;  /* alamat 0x40..0x77 (bit i = 0x40+i) */
volatile uint32_t g_scan_count  = 0;
volatile uint32_t g_first_addr  = 0;
volatile uint32_t g_done        = 0;
volatile uint32_t g_loop        = 0;

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

static void pins_mode(uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = mode;
    g.Pull = pull;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
}

static uint32_t read_idr(void) { return (GPIOB->IDR & (GPIO_PIN_6 | GPIO_PIN_7)); }

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

    /* ---- Tes 1: input + pull-up internal ---- */
    pins_mode(GPIO_MODE_INPUT, GPIO_PULLUP);
    HAL_Delay(20);
    g_t1_pullup = read_idr();

    /* ---- Tes 2: input tanpa pull ---- */
    pins_mode(GPIO_MODE_INPUT, GPIO_NOPULL);
    HAL_Delay(20);
    g_t2_nopull = read_idr();

    /* ---- Tes 3: output push-pull HIGH ---- */
    pins_mode(GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(20);
    g_t3_drive_hi = read_idr();

    /* ---- Tes 4: output push-pull LOW ---- */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(20);
    g_t4_drive_lo = read_idr();

    /* ---- Kembalikan ke I2C ---- */
    GPIO_InitTypeDef af = {0};
    af.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    af.Mode = GPIO_MODE_AF_OD;
    af.Pull = GPIO_PULLUP;
    af.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    af.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &af);

    __HAL_RCC_I2C1_CLK_ENABLE();
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
    HAL_Delay(50);

    /* ---- Scan semua alamat 0x08..0x77 ---- */
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 10) == HAL_OK) {
            if (a < 0x40) g_scan_lo |= (1u << (a - 0x08));
            else          g_scan_hi |= (1u << (a - 0x40));
            if (g_scan_count == 0) g_first_addr = a;
            g_scan_count++;
        }
    }

    g_done = 1;

    for (;;) {
        g_loop++;
        /* kedip cepat = selesai */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(950);
    }
}
