/**
 * slow_scan.c — Scan I2C bit-bang SANGAT LAMBAT dengan pull-up internal
 *
 * Tujuan: membuktikan apakah modul OLED masih hidup.
 *
 * Kalau modul punya pull-up 4.7k-10k, I2C normal jalan.
 * Kalau modul TIDAK punya pull-up, I2C normal gagal (line tidak pernah HIGH).
 * Tapi dengan pull-up internal MCU (~40k) + clock SANGAT lambat (~5 kHz),
 * rise time jadi tidak masalah, dan modul masih bisa ACK.
 *
 * HASIL:
 *   g_found = 0x3C atau 0x3D  => MODUL HIDUP, cuma butuh pull-up eksternal
 *   g_found = 0               => modul mati / tidak tersambung
 *
 * g_pullup_ok = 1 kalau dengan pull-up internal kedua line bisa HIGH
 */

#include "stm32f4xx_hal.h"

volatile uint32_t g_found      = 0;
volatile uint32_t g_pullup_ok  = 0;
volatile uint32_t g_idle_state = 0;
volatile uint32_t g_ack_3c     = 0;
volatile uint32_t g_ack_3d     = 0;
volatile uint32_t g_done       = 0;
volatile uint32_t g_loop       = 0;

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

/* ---------- bit-bang lambat, open-drain + pull-up internal ---------- */
#define SCL_H()   do { GPIOB->BSRR = GPIO_PIN_6; } while(0)
#define SCL_L()   do { GPIOB->BSRR = (uint32_t)GPIO_PIN_6 << 16; } while(0)
#define SDA_H()   do { GPIOB->BSRR = GPIO_PIN_7; } while(0)
#define SDA_L()   do { GPIOB->BSRR = (uint32_t)GPIO_PIN_7 << 16; } while(0)
#define SDA_READ() ((GPIOB->IDR & GPIO_PIN_7) ? 1u : 0u)
#define SCL_READ() ((GPIOB->IDR & GPIO_PIN_6) ? 1u : 0u)

/* ~5 kHz: 100 iterasi loop kosong kira-kira 10-20 us pada 16 MHz */
static void dly(void) { for (volatile int i = 0; i < 100; i++) { } }

static void bb_setup(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* open-drain + pull-up internal */
    GPIOB->OTYPER |= (GPIO_PIN_6 | GPIO_PIN_7);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u << 12) | (3u << 14)))
                 | ((1u << 12) | (1u << 14));          /* 01 = pull-up */
    GPIOB->OSPEEDR |= (3u << 12) | (3u << 14);
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 12) | (3u << 14)))
                 | ((1u << 12) | (1u << 14));          /* 01 = output */

    SCL_H(); SDA_H();
    dly(); dly(); dly();
}

static void bb_start(void)
{
    SDA_H(); dly();
    SCL_H(); dly();
    SDA_L(); dly();
    SCL_L(); dly();
}

static void bb_stop(void)
{
    SDA_L(); dly();
    SCL_H(); dly();
    SDA_H(); dly();
}

static int bb_write(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) SDA_H(); else SDA_L();
        dly();
        SCL_H(); dly();
        SCL_L(); dly();
    }
    /* clock ke-9: ACK */
    SDA_H(); dly();          /* lepas SDA, biarkan pull-up */
    SCL_H(); dly();
    int ack = (SDA_READ() == 0) ? 1 : 0;
    SCL_L(); dly();
    return ack;
}

static int bb_probe(uint8_t a7)
{
    bb_start();
    int ack = bb_write((uint8_t)(a7 << 1));
    bb_stop();
    dly(); dly();
    return ack;
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

    bb_setup();
    HAL_Delay(100);

    /* kondisi idle: kedua line harus HIGH kalau pull-up bekerja */
    g_idle_state = (GPIOB->IDR & (GPIO_PIN_6 | GPIO_PIN_7));
    g_pullup_ok  = (g_idle_state == (GPIO_PIN_6 | GPIO_PIN_7)) ? 1u : 0u;

    /* scan berulang beberapa kali untuk stabilitas */
    for (int attempt = 0; attempt < 5; attempt++) {
        if (bb_probe(0x3C)) { g_ack_3c = 1; }
        if (bb_probe(0x3D)) { g_ack_3d = 1; }
        HAL_Delay(20);
    }

    if (g_ack_3c)      g_found = 0x3C;
    else if (g_ack_3d) g_found = 0x3D;

    g_done = 1;

    for (;;) {
        g_loop++;
        /* kedip: 1x = tidak ketemu, 3x = ketemu 0x3C, 4x = ketemu 0x3D */
        int n = g_found ? (g_found == 0x3C ? 3 : 4) : 1;
        for (int i = 0; i < n; i++) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            HAL_Delay(120);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_Delay(180);
        }
        HAL_Delay(1200);
    }
}
