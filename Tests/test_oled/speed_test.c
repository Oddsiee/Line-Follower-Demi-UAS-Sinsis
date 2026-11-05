/**
 * speed_test.c — Cari kecepatan I2C yang bekerja dengan pull-up internal
 *
 * Menguji HAL I2C1 di PB6/PB7 pada beberapa kecepatan, lalu kalau ada yang
 * ACK, langsung init SH1106 dan isi layar putih.
 *
 * Hasil di g_* dibaca via SWD (mode hotplug, tanpa reset).
 */

#include "stm32f4xx_hal.h"

I2C_HandleTypeDef hi2c1;

volatile uint32_t g_ack[6]      = {0,0,0,0,0,0};  /* per kecepatan */
volatile uint32_t g_speed[6]    = {400000,200000,100000,50000,20000,10000};
volatile uint32_t g_best_speed  = 0;
volatile uint32_t g_best_addr   = 0;
volatile uint32_t g_init_ok     = 0;
volatile uint32_t g_white_ok    = 0;
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

static void i2c_setup(uint32_t speed)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;      /* WAJIB: tidak ada pull-up eksternal */
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = speed;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void wcmd(uint8_t a7, uint8_t c)
{
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x00,
                      I2C_MEMADD_SIZE_8BIT, &c, 1, 200);
}

static void wdata(uint8_t a7, const uint8_t *d, uint16_t n)
{
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x40,
                      I2C_MEMADD_SIZE_8BIT, (uint8_t *)d, n, 500);
}

static void init_sh1106(uint8_t a7)
{
    static const uint8_t s[] = {
        0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00, 0x40,
        0xAD,0x8B, 0xA1, 0xC8, 0xDA,0x12, 0x81,0xCF,
        0xD9,0x22, 0xDB,0x35, 0xA4, 0xA6, 0xAF
    };
    for (unsigned i = 0; i < sizeof(s); i++) wcmd(a7, s[i]);
}

static void fill_white(uint8_t a7)
{
    uint8_t line[128];
    for (int i = 0; i < 128; i++) line[i] = 0xFF;
    for (int p = 0; p < 8; p++) {
        wcmd(a7, (uint8_t)(0xB0 | p));
        wcmd(a7, 0x02);   /* SH1106 col offset */
        wcmd(a7, 0x10);
        wdata(a7, line, 128);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef led = {0};
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    led.Pin = GPIO_PIN_13; led.Mode = GPIO_MODE_OUTPUT_PP;
    led.Pull = GPIO_NOPULL; led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &led);

    /* --- uji tiap kecepatan --- */
    for (int i = 0; i < 6; i++) {
        i2c_setup(g_speed[i]);
        HAL_Delay(30);

        uint32_t ok = 0;
        if (HAL_I2C_IsDeviceReady(&hi2c1, (0x3C << 1), 3, 50) == HAL_OK) ok = 0x3C;
        else if (HAL_I2C_IsDeviceReady(&hi2c1, (0x3D << 1), 3, 50) == HAL_OK) ok = 0x3D;

        g_ack[i] = ok;

        if (ok && g_best_speed == 0) {
            g_best_speed = g_speed[i];
            g_best_addr  = ok;
        }
        HAL_I2C_DeInit(&hi2c1);
        HAL_Delay(20);
    }

    /* --- pakai kecepatan terbaik, init + layar putih --- */
    if (g_best_speed) {
        i2c_setup(g_best_speed);
        HAL_Delay(50);

        uint8_t a7 = (uint8_t)g_best_addr;
        init_sh1106(a7);
        g_init_ok = 1;

        fill_white(a7);
        g_white_ok = 1;
    }

    g_done = 1;

    for (;;) {
        g_loop++;
        /* kedip: 1x = gagal total, 2x = ada yang ACK */
        int n = g_best_speed ? 2 : 1;
        for (int i = 0; i < n; i++) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            HAL_Delay(120);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_Delay(180);
        }
        HAL_Delay(1000);
    }
}
