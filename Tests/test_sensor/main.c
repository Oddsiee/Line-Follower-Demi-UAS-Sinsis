/**
 * main.c — Test Sensor 16CH Dariyan-X (standalone, tanpa CubeIDE)
 *
 * Wiring (sesuai pinout master):
 *   PA4 = Out1 (ADC1_IN4)   PA5 = Out2 (ADC1_IN5)
 *   PA6 = Out3 (ADC1_IN6)   PA7 = Out4 (ADC1_IN7)
 *   PB0 = In1 (mux bit0)    PB1 = In2 (mux bit1)
 *   Sensor Vcc -> 3V3, Gnd -> GND
 *
 * Opsional:
 *   OLED  : PB6 = SCL, PB7 = SDA  (tampilan bar 16 kanal)
 *   UART  : PA15 = TX, PB3 = RX, 115200 8N1  (format utk Tools/uart_plot.py)
 *
 * Build : .\build.ps1
 * Flash : .\flash.ps1
 * Baca  : .\read_sensor.ps1   (baca g_raw[] via SWD, tanpa UART)
 */

#include "stm32f4xx_hal.h"
#include "sh1106.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 * Handle global (dipakai sh1106.c lewat extern)
 * ============================================================ */
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1;

/* ============================================================
 * Variabel hasil — dibaca via SWD oleh read_sensor.ps1
 * ============================================================ */
volatile uint16_t g_raw[16];        /* raw[group*4 + mux_state] */
volatile uint16_t g_min[16];
volatile uint16_t g_max[16];
volatile uint32_t g_scan_count = 0;
volatile uint32_t g_scan_us    = 0; /* durasi satu siklus scan penuh */
volatile uint32_t g_adc_err    = 0;
volatile uint32_t g_loop       = 0;

#define MUX_SETTLE_US 50U
#define OVERSAMPLE    4

void Error_Handler(void);

/* ============================================================
 * Delay presisi pakai DWT cycle counter
 * ============================================================ */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) { }
}

/* ============================================================
 * Clock: HSI 16 MHz (sama seperti test_oled)
 * ============================================================ */
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
 * GPIO: mux selector PB0/PB1, LED PC13
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PB0 = MUX_IN1, PB1 = MUX_IN2 -> output push-pull */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    g.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    /* PC13 = LED onboard (active low) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_13;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

/* ============================================================
 * ADC1: scan 4 kanal PA4..PA7 via DMA2 Stream0
 * ============================================================ */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA4..PA7 -> analog */
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_NORMAL;
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();

    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion   = 0;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 4;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    /* Kalibrasi manual. Paket HAL/CMSIS di mesin ini tidak menyediakan
     * HAL_ADCEx_Calibration_Start, jadi bit ADC_CR2 ditulis langsung.
     * Posisi bit sesuai RM0090 (STM32F4): ADON=0, CAL=2, RSTCAL=3.
     * Urutan: ADON=1 -> tstab -> RSTCAL -> CAL. */
    ADC1->CR2 |= (1UL << 0);                        /* ADON = 1 */
    for (volatile uint32_t d = 0; d < 1000; d++) { }/* tstab */

    ADC1->CR2 |= (1UL << 3);                        /* RSTCAL */
    for (uint32_t t = 0; (ADC1->CR2 & (1UL << 3)) && t < 100000UL; t++) { }

    ADC1->CR2 |= (1UL << 2);                        /* CAL */
    for (uint32_t t = 0; (ADC1->CR2 & (1UL << 2)) && t < 100000UL; t++) { }

    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ch.Offset       = 0;

    ch.Channel = ADC_CHANNEL_4; ch.Rank = 1; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_5; ch.Rank = 2; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_6; ch.Rank = 3; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_7; ch.Rank = 4; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
}

/* ============================================================
 * I2C1 (OLED) — PB6/PB7, 400 kHz
 * ============================================================ */
static void MX_I2C1_Init(void)
{
    /* WAJIB: tanpa clock, register I2C1 tidak bisa diakses */
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();

    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);
}

/* ============================================================
 * USART1 debug — PA15 = TX, PB3 = RX (AF7), 115200 8N1
 * ============================================================ */
static void MX_USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;

    g.Pin = GPIO_PIN_15;                 /* PA15 = USART1_TX */
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_3;                  /* PB3  = USART1_RX */
    HAL_GPIO_Init(GPIOB, &g);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void uart_printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 20);
    }
}

/* ============================================================
 * Scan sensor
 * ============================================================ */
static uint16_t s_adc[4];
static volatile uint8_t s_adc_done;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) s_adc_done = 1;
}

static void mux_set(uint8_t state)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,
                      (state & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,
                      (state & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool adc_read4(uint16_t out[4])
{
    s_adc_done = 0;
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc, 4) != HAL_OK) return false;

    uint32_t t0 = HAL_GetTick();
    while (!s_adc_done) {
        if ((HAL_GetTick() - t0) > 5U) {
            HAL_ADC_Stop_DMA(&hadc1);
            return false;
        }
    }
    HAL_ADC_Stop_DMA(&hadc1);

    for (int i = 0; i < 4; i++) out[i] = s_adc[i];
    return true;
}

static void sensor_scan(void)
{
    uint32_t t0 = DWT->CYCCNT;

    for (uint8_t s = 0; s < 4; s++) {
        mux_set(s);
        delay_us(MUX_SETTLE_US);

        /* Oversampling: baca OVERSAMPLE kali per state, lalu rata-rata.
         * Mengurangi noise high-frequency dari LED IR / switching supply. */
        uint32_t acc[4] = {0, 0, 0, 0};
        int n = 0;
        for (int k = 0; k < OVERSAMPLE; k++) {
            uint16_t v[4];
            if (adc_read4(v)) {
                for (int g = 0; g < 4; g++) acc[g] += v[g];
                n++;
            }
        }
        if (n == 0) { g_adc_err++; n = 1; }

        for (int g = 0; g < 4; g++) {
            uint16_t val = (uint16_t)(acc[g] / (uint32_t)n);
            int idx = g * 4 + s;
            g_raw[idx] = val;
            if (val < g_min[idx]) g_min[idx] = val;
            if (val > g_max[idx]) g_max[idx] = val;
        }
    }

    g_scan_us = (DWT->CYCCNT - t0) / (SystemCoreClock / 1000000U);
    g_scan_count++;

    /* Reset statistik min/max tiap 1000 scan supaya selalu mencerminkan
     * kondisi terkini (bukan menumpuk dari kondisi wiring lama). */
    if ((g_scan_count % 1000U) == 0U) {
        for (int i = 0; i < 16; i++) { g_min[i] = 0xFFFF; g_max[i] = 0; }
    }
}

/* ============================================================
 * MODE DIAGNOSTIK (build dengan -DDIAG_MODE)
 *
 * Menjawab: apakah nilai tidak stabil karena (a) mux belum settle,
 * (b) sensor/modul memang noisy, atau (c) state tertentu saturasi.
 *
 * Hasil di g_dmin/g_dmax/g_dmean[16] (indeks g*4+s) dan
 * g_fix_min/g_fix_max[4] (uji state tetap, tanpa ganti mux).
 * ============================================================ */
#ifdef DIAG_MODE
volatile uint16_t g_dmin[16];
volatile uint16_t g_dmax[16];
volatile uint16_t g_dmean[16];
volatile uint16_t g_fix_min[4];
volatile uint16_t g_fix_max[4];
volatile uint32_t g_diag_done = 0;
volatile uint16_t g_vref_min = 0xFFFF;
volatile uint16_t g_vref_max = 0;
volatile uint16_t g_vref_mean = 0;
volatile uint16_t g_poll_min[4];
volatile uint16_t g_poll_max[4];
volatile uint16_t g_poll_mean[4];

/* Baca PA4..PA7 pakai POLLING (tanpa DMA), mux ditahan di state 0.
 * Kalau ini STABIL sementara versi DMA kacau -> bug di jalur DMA.
 * Kalau ini juga kacau -> masalah hardware (ground/sensor). */
static void diag_polling(void)
{
    for (int i = 0; i < 4; i++) { g_poll_min[i] = 0xFFFF; g_poll_max[i] = 0; g_poll_mean[i] = 0; }

    /* Polling per-kanal butuh EOC per konversi, bukan per sequence. */
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    mux_set(0);
    delay_us(5000);

    uint32_t sum[4] = {0, 0, 0, 0};
    int n = 0;

    for (int k = 0; k < 200; k++) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK) continue;

        uint16_t v[4];
        int ok = 1;
        for (int g = 0; g < 4; g++) {
            if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) { ok = 0; break; }
            v[g] = (uint16_t)HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);

        if (ok) {
            for (int g = 0; g < 4; g++) {
                if (v[g] < g_poll_min[g]) g_poll_min[g] = v[g];
                if (v[g] > g_poll_max[g]) g_poll_max[g] = v[g];
                sum[g] += v[g];
            }
            n++;
        }
    }
    if (n > 0) {
        for (int g = 0; g < 4; g++) g_poll_mean[g] = (uint16_t)(sum[g] / (uint32_t)n);
    }

    /* kembalikan EOCSelection untuk mode DMA */
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    HAL_ADC_Init(&hadc1);
}

/* Baca VREFINT (channel 17) — referensi internal MCU, TIDAK tergantung
 * sensor. Kalau ini stabil (~1500 @ 3.3V) berarti ADC + suplai MCU sehat,
 * dan masalahnya murni di sisi sensor. */
static void diag_vrefint(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    uint32_t sum = 0;
    int n = 0;

    /* aktifkan VREFINT */
    ADC->CCR |= (1UL << 23);            /* TSVREFE */

    ch.Channel      = ADC_CHANNEL_VREFINT;
    ch.Rank         = 1;
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ch.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.ScanConvMode    = DISABLE;
    HAL_ADC_Init(&hadc1);

    for (int k = 0; k < 100; k++) {
        uint16_t v = 0;
        if (HAL_ADC_Start(&hadc1) == HAL_OK) {
            if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
                v = (uint16_t)HAL_ADC_GetValue(&hadc1);
                if (v < g_vref_min) g_vref_min = v;
                if (v > g_vref_max) g_vref_max = v;
                sum += v;
                n++;
            }
            HAL_ADC_Stop(&hadc1);
        }
    }
    if (n > 0) g_vref_mean = (uint16_t)(sum / (uint32_t)n);

    /* --- kembalikan konfigurasi 4 kanal PA4..PA7 --- */
    hadc1.Init.NbrOfConversion = 4;
    hadc1.Init.ScanConvMode    = ENABLE;
    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef c2 = {0};
    c2.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    c2.Offset       = 0;
    c2.Channel = ADC_CHANNEL_4; c2.Rank = 1; HAL_ADC_ConfigChannel(&hadc1, &c2);
    c2.Channel = ADC_CHANNEL_5; c2.Rank = 2; HAL_ADC_ConfigChannel(&hadc1, &c2);
    c2.Channel = ADC_CHANNEL_6; c2.Rank = 3; HAL_ADC_ConfigChannel(&hadc1, &c2);
    c2.Channel = ADC_CHANNEL_7; c2.Rank = 4; HAL_ADC_ConfigChannel(&hadc1, &c2);
}

static void diag_run(void)
{
    for (int i = 0; i < 16; i++) { g_dmin[i] = 0xFFFF; g_dmax[i] = 0; g_dmean[i] = 0; }
    for (int i = 0; i < 4; i++)  { g_fix_min[i] = 0xFFFF; g_fix_max[i] = 0; }

    /* --- Uji 0: VREFINT (referensi internal, tidak tergantung sensor) --- */
    diag_vrefint();

    /* --- Uji 0b: polling tanpa DMA, mux tetap --- */
    diag_polling();

    /* --- Uji 1: state TETAP (0), 200 baca. Kalau spread besar di sini,
     *     masalahnya bukan settling mux tapi sensor/ADC. --- */
    mux_set(0);
    delay_us(5000);
    for (int k = 0; k < 200; k++) {
        uint16_t v[4];
        if (adc_read4(v)) {
            for (int g = 0; g < 4; g++) {
                if (v[g] < g_fix_min[g]) g_fix_min[g] = v[g];
                if (v[g] > g_fix_max[g]) g_fix_max[g] = v[g];
            }
        }
    }

    /* --- Uji 2: karakterisasi tiap state, 100 sampel, settle 5 ms --- */
    for (int s = 0; s < 4; s++) {
        mux_set((uint8_t)s);
        delay_us(5000);
        uint32_t sum[4] = {0, 0, 0, 0};
        int n = 0;
        for (int k = 0; k < 100; k++) {
            uint16_t v[4];
            if (adc_read4(v)) {
                for (int g = 0; g < 4; g++) {
                    int idx = g * 4 + s;
                    if (v[g] < g_dmin[idx]) g_dmin[idx] = v[g];
                    if (v[g] > g_dmax[idx]) g_dmax[idx] = v[g];
                    sum[g] += v[g];
                }
                n++;
            }
        }
        if (n == 0) n = 1;
        for (int g = 0; g < 4; g++) g_dmean[g * 4 + s] = (uint16_t)(sum[g] / (uint32_t)n);
    }

    g_diag_done = 1;
}
#endif /* DIAG_MODE */

/* ============================================================
 * Tampilan OLED
 * ============================================================ */
static void draw_screen(void)
{
    SH1106_Clear();
    SH1106_DrawString(0, 0, "SENSOR 16CH");

    /* bar 16 kanal, skala 0..4095 -> 0..22 px */
    const int barW = 6, gap = 2, x0 = 4, base = 30;
    for (int i = 0; i < 16; i++) {
        int v = (int)((uint32_t)g_raw[i] * 22U / 4095U);
        int x = x0 + i * (barW + gap);
        if (v > 0) SH1106_DrawFillRect(x, base - v, barW, v, true);
    }

    /* 4 baris x 4 nilai */
    char buf[24];
    for (int r = 0; r < 4; r++) {
        buf[0] = '\0';
        for (int c = 0; c < 4; c++) {
            char t[8];
            snprintf(t, sizeof(t), "%4u", (unsigned)g_raw[r * 4 + c]);
            strcat(buf, t);
        }
        SH1106_DrawString(0, 32 + r * 8, buf);
    }

    SH1106_Update();
}

/* ============================================================
 * main
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    dwt_init();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    MX_USART1_Init();

    for (int i = 0; i < 16; i++) { g_min[i] = 0xFFFF; g_max[i] = 0; }

    /* OLED opsional: kalau tidak ada, driver tetap aman (timeout) */
    if (SH1106_Probe(0x3C))      SH1106_SetAddress(0x3C);
    else if (SH1106_Probe(0x3D)) SH1106_SetAddress(0x3D);
    SH1106_Init();

    uint32_t last_draw = 0;
    uint32_t last_uart = 0;

#ifdef DIAG_MODE
    diag_run();
    for (;;) { HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); HAL_Delay(500); }
#endif

    for (;;) {
        sensor_scan();
        g_loop++;

        uint32_t now = HAL_GetTick();

        if (now - last_draw >= 100U) {
            last_draw = now;
            draw_screen();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        if (now - last_uart >= 100U) {
            last_uart = now;
            /* format utk Tools/uart_plot.py: S,v0,...,v15,pos */
            uart_printf("S,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,0\r\n",
                        g_raw[0],  g_raw[1],  g_raw[2],  g_raw[3],
                        g_raw[4],  g_raw[5],  g_raw[6],  g_raw[7],
                        g_raw[8],  g_raw[9],  g_raw[10], g_raw[11],
                        g_raw[12], g_raw[13], g_raw[14], g_raw[15]);
        }
    }
}
