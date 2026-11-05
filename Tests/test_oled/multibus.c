/**
 * multibus.c — Scan I2C di SEMUA kemungkinan pin pair + bit-bang
 *
 * Menjawab: apakah OLED sebenarnya terpasang di pin selain PB6/PB7?
 *
 * Bus yang diuji:
 *   A. Bit-bang software di PB6/PB7   (bebas dari masalah konfigurasi HAL)
 *   B. Hardware I2C1 di PB6/PB7       (AF4)  <- sesuai dokumentasi
 *   C. Hardware I2C1 di PB8/PB9       (AF4)
 *   D. Hardware I2C2 di PB10/PB11     (AF4)
 *   E. Hardware I2C3 di PA8/PC9       (AF4)
 *
 * Hasil: g_bus[0..4] = alamat yang ACK (0 = tidak ada)
 */

#include "stm32f4xx_hal.h"

I2C_HandleTypeDef h;

volatile uint32_t g_bus[5]      = {0,0,0,0,0};
volatile uint32_t g_bus_count[5]= {0,0,0,0,0};
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

/* ============================================================
 * Bit-bang I2C di PB6 (SCL) / PB7 (SDA)
 * ============================================================ */
#define SCL_H()  do { GPIOB->BSRR = GPIO_PIN_6; } while(0)
#define SCL_L()  do { GPIOB->BSRR = (uint32_t)GPIO_PIN_6 << 16; } while(0)
#define SDA_H()  do { GPIOB->BSRR = GPIO_PIN_7; } while(0)
#define SDA_L()  do { GPIOB->BSRR = (uint32_t)GPIO_PIN_7 << 16; } while(0)
#define SDA_IN() do { GPIOB->MODER = (GPIOB->MODER & ~(3u << 14)) | (0u << 14); } while(0)
#define SDA_OUT()do { GPIOB->MODER = (GPIOB->MODER & ~(3u << 14)) | (1u << 14); } while(0)
#define SDA_READ() ((GPIOB->IDR & GPIO_PIN_7) ? 1u : 0u)

static void bb_delay(void) { for (volatile int i = 0; i < 40; i++) { } }

static void bb_start(void)
{
    SDA_OUT(); SDA_H(); SCL_H(); bb_delay();
    SDA_L(); bb_delay();
    SCL_L(); bb_delay();
}

static void bb_stop(void)
{
    SDA_OUT(); SDA_L(); bb_delay();
    SCL_H(); bb_delay();
    SDA_H(); bb_delay();
}

/* return 1 kalau ACK */
static int bb_write(uint8_t b)
{
    SDA_OUT();
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) SDA_H(); else SDA_L();
        bb_delay();
        SCL_H(); bb_delay();
        SCL_L(); bb_delay();
    }
    /* clock ke-9: baca ACK */
    SDA_IN(); bb_delay();
    SCL_H(); bb_delay();
    int ack = (SDA_READ() == 0) ? 1 : 0;
    SCL_L(); bb_delay();
    SDA_OUT();
    return ack;
}

static void bb_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* PB6 & PB7: output open-drain, pull-up */
    GPIOB->OTYPER |= (GPIO_PIN_6 | GPIO_PIN_7);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u<<12)|(3u<<14))) | ((1u<<12)|(1u<<14));
    GPIOB->OSPEEDR |= (3u<<12)|(3u<<14);
    GPIOB->MODER = (GPIOB->MODER & ~((3u<<12)|(3u<<14))) | ((1u<<12)|(1u<<14));
    SCL_H(); SDA_H(); bb_delay();
}

static void bb_scan(void)
{
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        bb_start();
        int ack = bb_write((uint8_t)(a << 1));
        bb_stop();
        if (ack) {
            if (g_bus_count[0] < 8) g_bus[0] = (g_bus[0] << 8) | a;
            g_bus_count[0]++;
        }
        bb_delay();
    }
}

/* ============================================================
 * Hardware I2C scan
 * ============================================================ */
static void hw_scan(I2C_TypeDef *inst, int idx,
                    GPIO_TypeDef *port, uint32_t scl, uint32_t sda, uint32_t af)
{
    GPIO_InitTypeDef g = {0};

    if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin = scl | sda;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = af;
    HAL_GPIO_Init(port, &g);

    if (inst == I2C1) __HAL_RCC_I2C1_CLK_ENABLE();
    if (inst == I2C2) __HAL_RCC_I2C2_CLK_ENABLE();
    if (inst == I2C3) __HAL_RCC_I2C3_CLK_ENABLE();

    h.Instance             = inst;
    h.Init.ClockSpeed      = 100000;
    h.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    h.Init.OwnAddress1     = 0;
    h.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    h.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    h.Init.OwnAddress2     = 0;
    h.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    h.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&h) != HAL_OK) return;

    HAL_Delay(20);

    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (HAL_I2C_IsDeviceReady(&h, (uint16_t)(a << 1), 2, 10) == HAL_OK) {
            if (g_bus_count[idx] < 8) g_bus[idx] = (g_bus[idx] << 8) | a;
            g_bus_count[idx]++;
        }
    }
    HAL_I2C_DeInit(&h);
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

    /* A. bit-bang PB6/PB7 */
    bb_init();
    bb_scan();

    /* B. I2C1 PB6/PB7 */
    hw_scan(I2C1, 1, GPIOB, GPIO_PIN_6, GPIO_PIN_7, GPIO_AF4_I2C1);

    /* C. I2C1 PB8/PB9 */
    hw_scan(I2C1, 2, GPIOB, GPIO_PIN_8, GPIO_PIN_9, GPIO_AF4_I2C1);

    /* D. I2C2 PB10/PB11 */
    hw_scan(I2C2, 3, GPIOB, GPIO_PIN_10, GPIO_PIN_11, GPIO_AF4_I2C2);

    /* E. I2C3 PA8/PC9 */
    {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        g.Pin = GPIO_PIN_8; g.Mode = GPIO_MODE_AF_OD; g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF4_I2C3;
        HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOC, &g);
    }
    hw_scan(I2C3, 4, GPIOA, GPIO_PIN_8, GPIO_PIN_8, GPIO_AF4_I2C3);

    g_done = 1;

    for (;;) {
        g_loop++;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(950);
    }
}
