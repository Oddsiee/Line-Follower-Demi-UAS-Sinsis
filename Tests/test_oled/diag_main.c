/**
 * diag_main.c — Diagnostik OLED (tanpa asumsi)
 *
 * Tujuan: memisahkan 3 kemungkinan penyebab layar gelap:
 *   A. OLED tidak ACK di I2C sama sekali  -> masalah wiring / daya / alamat
 *   B. OLED ACK tapi tetap gelap          -> init sequence salah (SH1106 vs SSD1306)
 *   C. OLED ACK dan menyala               -> driver asli yang bermasalah
 *
 * Cara baca hasil:
 *   1. LED PC13 berkedip = hasil scan I2C
 *        1x = tidak ada device
 *        2x = ada device di 0x3C
 *        3x = ada device di 0x3D
 *   2. Layar bergantian tiap 3 detik:
 *        fase 0 = init SH1106  + layar putih penuh
 *        fase 1 = init SSD1306 + layar putih penuh
 *      Kalau salah satu fase membuat layar putih -> chipset-nya itu.
 *
 * Variabel g_* bisa dibaca lewat SWD (lihat diag.ps1).
 */

#include "stm32f4xx_hal.h"

I2C_HandleTypeDef hi2c1;

/* --- hasil diagnosa, dibaca via SWD --- */
volatile uint32_t g_scan_done   = 0;
volatile uint32_t g_ack_100k    = 0;   /* bit0=0x3C, bit1=0x3D */
volatile uint32_t g_ack_400k    = 0;   /* bit0=0x3C, bit1=0x3D */
volatile uint32_t g_found_3c    = 0;
volatile uint32_t g_found_3d    = 0;
volatile uint32_t g_cmd_status  = 0xDEADBEEF;
volatile uint32_t g_data_status = 0xDEADBEEF;
volatile uint32_t g_phase       = 0;
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

static void gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PB6/PB7 -> I2C1 AF4, open-drain, pull-up internal */
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    /* PC13 = LED onboard, active low */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_13;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

static void i2c_init(uint32_t speed)
{
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

static uint32_t probe(uint8_t a7)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a7 << 1), 3, 25) == HAL_OK) ? 1u : 0u;
}

static void led_blink(int n)
{
    for (int i = 0; i < n; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(200);
    }
    HAL_Delay(700);
}

static void wcmd(uint8_t a7, uint8_t c)
{
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x00,
                      I2C_MEMADD_SIZE_8BIT, &c, 1, 100);
}

static void wdata(uint8_t a7, const uint8_t *d, uint16_t n)
{
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x40,
                      I2C_MEMADD_SIZE_8BIT, (uint8_t *)d, n, 200);
}

/* Isi seluruh layar putih. col_offset = 2 untuk SH1106, 0 untuk SSD1306. */
static void fill_white(uint8_t a7, int col_offset)
{
    uint8_t line[128];
    for (int i = 0; i < 128; i++) line[i] = 0xFF;

    for (int p = 0; p < 8; p++) {
        wcmd(a7, (uint8_t)(0xB0 | p));
        wcmd(a7, (uint8_t)(0x00 | (col_offset & 0x0F)));
        wcmd(a7, (uint8_t)(0x10 | ((col_offset >> 4) & 0x0F)));
        wdata(a7, line, 128);
    }
}

static void init_sh1106(uint8_t a7)
{
    static const uint8_t s[] = {
        0xAE,          /* display off        */
        0xD5, 0x80,    /* clock divide       */
        0xA8, 0x3F,    /* multiplex 64       */
        0xD3, 0x00,    /* display offset     */
        0x40,          /* start line 0       */
        0xAD, 0x8B,    /* charge pump ON     */
        0xA1,          /* segment remap      */
        0xC8,          /* COM scan remapped  */
        0xDA, 0x12,    /* COM pins           */
        0x81, 0xCF,    /* contrast           */
        0xD9, 0x22,    /* pre-charge         */
        0xDB, 0x35,    /* VCOM               */
        0xA4,          /* resume             */
        0xA6,          /* normal             */
        0xAF           /* display on         */
    };
    for (unsigned i = 0; i < sizeof(s); i++) wcmd(a7, s[i]);
}

static void init_ssd1306(uint8_t a7)
{
    static const uint8_t s[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,    /* charge pump ON (SSD1306) */
        0x20, 0x00,    /* memory mode horizontal   */
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0xCF,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0xAF
    };
    for (unsigned i = 0; i < sizeof(s); i++) wcmd(a7, s[i]);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    gpio_init();

    /* --- scan I2C di dua kecepatan --- */
    i2c_init(100000);
    HAL_Delay(50);
    g_ack_100k = probe(0x3C) | (probe(0x3D) << 1);

    i2c_init(400000);
    HAL_Delay(50);
    g_ack_400k = probe(0x3C) | (probe(0x3D) << 1);

    g_found_3c  = probe(0x3C);
    g_found_3d  = probe(0x3D);
    g_scan_done = 1;

    uint8_t a7 = g_found_3c ? 0x3C : (g_found_3d ? 0x3D : 0x3C);

    /* --- uji tulis perintah & data --- */
    uint8_t c = 0xAE;
    g_cmd_status = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x00,
                                     I2C_MEMADD_SIZE_8BIT, &c, 1, 100);
    uint8_t d4[4] = {0, 0, 0, 0};
    g_data_status = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(a7 << 1), 0x40,
                                      I2C_MEMADD_SIZE_8BIT, d4, 4, 100);

    for (;;)
    {
        g_loop++;

        if (!g_found_3c && !g_found_3d) {
            led_blink(1);          /* tidak ada device sama sekali */
            continue;
        }

        /* fase 0: SH1106 */
        g_phase = 0;
        led_blink(g_found_3c ? 2 : 3);
        init_sh1106(a7);
        fill_white(a7, 2);
        HAL_Delay(3000);

        /* fase 1: SSD1306 */
        g_phase = 1;
        led_blink(g_found_3c ? 2 : 3);
        init_ssd1306(a7);
        fill_white(a7, 0);
        HAL_Delay(3000);
    }
}
