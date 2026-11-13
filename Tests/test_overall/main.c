/**
 * main.c — Test Overall Hardware Robot Line Follower TED 2026 (standalone)
 *
 * Menguji seluruh subsistem secara interaktif lewat 4 tombol + OLED SH1106,
 * dirancang untuk pengujian mandiri menggunakan suplai buck converter (tanpa ST-Link):
 *   1. Test Tombol       (PA7=UP, PA6=DOWN, PA5=OK, PA4=BACK)
 *   2. Test Sensor 16CH  (PA0/PA1/PA2/PB1 ADC1 + PA3/PB0 Mux + posisi garis)
 *   3. Test Motor ZK-5AD (PA8..PA11 TIM1 CH1..CH4, mode aman STOP saat masuk)
 *   4. Test OLED & Power (Monitor tegangan 3.3V VREFINT, statistik I2C, pola piksel)
 *
 * Build : .\build.ps1
 * Flash : .\flash.ps1
 */

#include "stm32f4xx_hal.h"
#include "config.h"
#include "buttons.h"
#include "motor.h"
#include "sh1106.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 * Handle global — dipakai motor.c (htim1) & sh1106.c (hi2c1)
 * ============================================================ */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1;

/* ============================================================
 * Variabel status global (dapat dipantau via SWD jika terhubung)
 * ============================================================ */
typedef enum {
    MODE_MENU = 0,
    MODE_BUTTONS,
    MODE_SENSOR,
    MODE_MOTOR,
    MODE_OLED_PWR
} TestMode_t;

volatile uint8_t  g_mode        = MODE_MENU;
volatile uint32_t g_loop        = 0;
volatile uint8_t  g_oled_ok     = 0;
volatile uint8_t  g_oled_addr   = 0x3C;

volatile uint16_t g_raw[SENSOR_COUNT];
volatile int16_t  g_pos         = 0;
volatile uint8_t  g_line_vis    = 0;
volatile uint32_t g_scan_us     = 0;
volatile uint32_t g_adc_err     = 0;

volatile uint16_t g_vdda_mv     = 0;
volatile uint16_t g_vdda_min_mv = 0xFFFFU;
volatile uint16_t g_vdda_max_mv = 0;

volatile int16_t  g_cmd_left    = 0;
volatile int16_t  g_cmd_right   = 0;
volatile uint8_t  g_motor_armed = 0;
volatile uint8_t  g_motor_phase = 0;

volatile uint32_t g_btn_cnt[4]  = {0, 0, 0, 0};
volatile uint8_t  g_btn_ready   = 0;
volatile uint8_t  g_btn_raw_mask = 0;

volatile uint32_t g_i2c_pd      = 0;   /* PB6/PB7 IDR dgn pull-down (0xC0 = pull-up eksternal aktif) */
volatile uint32_t g_i2c_pu      = 0;   /* PB6/PB7 IDR dgn pull-up */
volatile uint32_t g_i2c_fl      = 0;   /* PB6/PB7 IDR floating */

#define OVERALL_MUX_SETTLE_US  50U
#define OVERALL_OVERSAMPLE     4U
#define MENU_ITEM_COUNT        4U
#define OLED_SENSOR_MS         80U
#define OLED_BG_REFRESH_MS     400U
#define VDDA_SAMPLE_MS         50U
#define BOOT_SETTLE_MS         150U
#define BOOT_SPLASH_MS         600U
#define BTN_DEBOUNCE_MS        12U
#define BTN_LONGPRESS_MS       500U

void Error_Handler(void);

/* ============================================================
 * DWT microsecond timer untuk settling mux
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
 * Clock: HSI 16 MHz (konsisten dengan semua firmware uji)
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
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
}

/* ============================================================
 * GPIO:
 *   PA3 (MUX_IN1), PB0 (MUX_IN2) -> output push-pull
 *   PA8..PA11                    -> AF1 TIM1_CH1..CH4
 *   PB6..PB7                     -> AF4 I2C1
 *   PA4..PA7                     -> input pull-up (tombol)
 *   PC13                         -> LED onboard (active low)
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* MUX_IN1 = PA3 */
    HAL_GPIO_WritePin(MUX_IN1_PORT, MUX_IN1_PIN, GPIO_PIN_RESET);
    g.Pin   = MUX_IN1_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MUX_IN1_PORT, &g);

    /* MUX_IN2 = PB0 */
    HAL_GPIO_WritePin(MUX_IN2_PORT, MUX_IN2_PIN, GPIO_PIN_RESET);
    g.Pin   = MUX_IN2_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MUX_IN2_PORT, &g);

    /* PA8..PA11 = TIM1_CH1..CH4 */
    g.Pin       = MOTOR_D0_PIN | MOTOR_D1_PIN | MOTOR_D2_PIN | MOTOR_D3_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &g);

    /* PB6/PB7 = I2C1 OLED */
    g.Pin       = OLED_SCL_PIN | OLED_SDA_PIN;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = OLED_I2C_AF;
    HAL_GPIO_Init(GPIOB, &g);

    /* PA4..PA7 = 4 tombol menu, aktif LOW */
    g.Pin       = BTN_UP_PIN | BTN_DOWN_PIN | BTN_OK_PIN | BTN_BACK_PIN;
    g.Mode      = GPIO_MODE_INPUT;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = 0;
    HAL_GPIO_Init(BTN_PORT, &g);

    /* PC13 = LED heartbeat (active low, mulai mati) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_13;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

/* ============================================================
 * TIM1: PWM 16 kHz (PSC=0, ARR=999)
 * ============================================================ */
static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};
    TIM_OC_InitTypeDef      sConfigOC          = {0};

    __HAL_RCC_TIM1_CLK_ENABLE();

    htim1.Instance               = MOTOR_TIM;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = MOTOR_PWM_ARR;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) { Error_Handler(); }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) { Error_Handler(); }

    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) { Error_Handler(); }
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) { Error_Handler(); }
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) { Error_Handler(); }
}

/* ============================================================
 * Diagnostik pin I2C PB6/PB7 (pull-down / pull-up / float)
 * ============================================================ */
static void i2c_pin_diag(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_Delay(5);
    g_i2c_pd = (GPIOB->IDR & (OLED_SCL_PIN | OLED_SDA_PIN));

    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_Delay(5);
    g_i2c_pu = (GPIOB->IDR & (OLED_SCL_PIN | OLED_SDA_PIN));

    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_Delay(5);
    g_i2c_fl = (GPIOB->IDR & (OLED_SCL_PIN | OLED_SDA_PIN));

    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = OLED_I2C_AF;
    HAL_GPIO_Init(GPIOB, &g);
}

/* ============================================================
 * I2C1: PB6/PB7, 100 kHz (stabil dengan pull-up internal PB6)
 * ============================================================ */
static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = OLED_I2C;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }

    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin       = OLED_SCL_PIN | OLED_SDA_PIN;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = OLED_I2C_AF;
    HAL_GPIO_Init(GPIOB, &g);
}

/* ============================================================
 * ADC1: 5 kanal (Out1..Out4 + VREFINT) via DMA2 Stream0
 * ============================================================ */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) { return; }

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;  /* PA0, PA1, PA2 */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin  = GPIO_PIN_1;                            /* PB1 */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &g);

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
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) { Error_Handler(); }

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
    hadc1.Init.NbrOfConversion       = 5;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }

    /* Aktifkan referensi internal VREFINT (channel 17) */
    ADC->CCR |= ADC_CCR_TSVREFE;

    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ch.Offset       = 0;

    ch.Channel = SENS_ADC_CH_OUT1;    ch.Rank = 1; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) { Error_Handler(); }
    ch.Channel = SENS_ADC_CH_OUT2;    ch.Rank = 2; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) { Error_Handler(); }
    ch.Channel = SENS_ADC_CH_OUT3;    ch.Rank = 3; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) { Error_Handler(); }
    ch.Channel = SENS_ADC_CH_OUT4;    ch.Rank = 4; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) { Error_Handler(); }
    ch.Channel = ADC_CHANNEL_VREFINT; ch.Rank = 5; if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}

/* ============================================================
 * Sensor 16CH + VREFINT reader
 * ============================================================ */
static uint16_t s_adc[5];
static volatile uint8_t s_adc_done = 0;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) { s_adc_done = 1; }
}

static void mux_set(uint8_t state)
{
    HAL_GPIO_WritePin(MUX_IN1_PORT, MUX_IN1_PIN,
                      (state & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_IN2_PORT, MUX_IN2_PIN,
                      (state & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool adc_read5(uint16_t out[5])
{
    s_adc_done = 0;
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc, 5) != HAL_OK) { return false; }

    uint32_t t0 = HAL_GetTick();
    while (!s_adc_done) {
        if ((HAL_GetTick() - t0) > 10U) {
            HAL_ADC_Stop_DMA(&hadc1);
            return false;
        }
    }
    HAL_ADC_Stop_DMA(&hadc1);

    for (int i = 0; i < 5; i++) { out[i] = s_adc[i]; }
    return true;
}

static void vdda_update(uint16_t vref_raw)
{
    if (vref_raw == 0U) { return; }

    uint16_t vref_cal = *(const uint16_t *)0x1FFF7A2AUL;
    uint32_t mv;
    if (vref_cal >= 1300U && vref_cal <= 1700U) {
        mv = (3300UL * (uint32_t)vref_cal) / (uint32_t)vref_raw;
    } else {
        mv = (1210UL * 4095UL) / (uint32_t)vref_raw;
    }

    if (mv > 6000UL) { mv = 6000UL; }
    g_vdda_mv = (uint16_t)mv;
    if (g_vdda_mv < g_vdda_min_mv) { g_vdda_min_mv = g_vdda_mv; }
    if (g_vdda_mv > g_vdda_max_mv) { g_vdda_max_mv = g_vdda_mv; }
}

static void sample_vdda_only(void)
{
    uint32_t acc = 0;
    uint32_t ok  = 0;
    for (uint32_t k = 0; k < OVERALL_OVERSAMPLE; k++) {
        uint16_t v[5];
        if (adc_read5(v)) {
            acc += v[4];
            ok++;
        }
    }
    if (ok > 0U) {
        vdda_update((uint16_t)(acc / ok));
    }
}

static void sensor_scan_full(void)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t vref_acc = 0;
    uint32_t vref_n   = 0;

    for (uint8_t s = 0; s < MUX_STATES; s++) {
        mux_set(s);
        delay_us(OVERALL_MUX_SETTLE_US);

        uint32_t acc[4] = {0, 0, 0, 0};
        uint32_t n = 0;
        for (uint32_t k = 0; k < OVERALL_OVERSAMPLE; k++) {
            uint16_t v[5];
            if (adc_read5(v)) {
                for (int g = 0; g < SENSOR_GROUPS; g++) {
                    acc[g] += v[g];
                }
                vref_acc += v[4];
                vref_n++;
                n++;
            }
        }
        if (n == 0U) {
            g_adc_err++;
            n = 1U;
        }
        for (int g = 0; g < SENSOR_GROUPS; g++) {
            g_raw[g * MUX_STATES + s] = (uint16_t)(acc[g] / n);
        }
    }

    if (vref_n > 0U) {
        vdda_update((uint16_t)(vref_acc / vref_n));
    }

    /* Hitung posisi garis (weighted average -7500..+7500) */
    int32_t sum_w  = 0;
    int32_t sum_iw = 0;
    uint8_t vis    = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        uint16_t val = g_raw[i];
        if (val >= SENSOR_ON_LINE_MIN) {
            sum_w  += (int32_t)val;
            sum_iw += (int32_t)val * i;
            vis     = 1U;
        }
    }
    g_line_vis = vis;
    if (sum_w > 0) {
        int32_t idx_x10 = (sum_iw * 10) / sum_w;
        g_pos = (int16_t)((idx_x10 - 75) * (SENSOR_POS_SCALE / 10));
    } else {
        g_pos = 0;
    }

    g_scan_us = (DWT->CYCCNT - t0) / (SystemCoreClock / 1000000U);
}

/* ============================================================
 * OLED helper
 * ============================================================ */
static void oled_init_safe(void)
{
    /* Init pertama melakukan SWRST bus recovery bila I2C sempat macet */
    SH1106_Init();

    bool a3c = SH1106_Probe(0x3C);
    bool a3d = SH1106_Probe(0x3D);
    g_sh1106_scan_count = (a3c ? 1U : 0U) + (a3d ? 1U : 0U);
    if (a3c || a3d) {
        g_oled_ok   = 1U;
        g_oled_addr = a3c ? 0x3C : 0x3D;
        SH1106_SetAddress(g_oled_addr);
        SH1106_Init();
    } else {
        g_oled_ok = 0U;
    }
}

static void draw_boot_splash(void)
{
    if (!g_oled_ok) { return; }
    char buf[24];

    SH1106_Clear();
    SH1106_DrawString(0,  4, "LF TED 2026 - SV UGM");
    SH1106_DrawLine(0, 14, 127, 14, true);
    SH1106_DrawString(0, 22, "OVERALL HW TEST");
    SH1106_DrawString(0, 34, "OLED/BTN/SENS/MOTOR");
    snprintf(buf, sizeof(buf), "Buck 3V3: %4u mV", (unsigned)g_vdda_mv);
    SH1106_DrawString(0, 50, buf);
    SH1106_Update();
}

/* ============================================================
 * State masing-masing mode
 * ============================================================ */
static uint8_t s_menu_idx = 0;
static volatile uint8_t s_oled_dirty = 1U;

/* ============================================================
 * Sampling tombol 1 kHz (dipanggil dari SysTick_Handler) + FIFO 8 event
 * ============================================================ */
typedef struct {
    uint8_t btn;
    uint8_t is_long;
} BtnEvt_t;

typedef struct {
    uint8_t  stable;
    uint8_t  last_raw;
    uint8_t  stable_cnt;
    uint8_t  long_fired;
    uint32_t press_ms;
} FastBtnState_t;

static volatile BtnEvt_t s_evt_q[8];
static volatile uint8_t  s_evt_head = 0;
static volatile uint8_t  s_evt_tail = 0;
static FastBtnState_t    s_fbtn[4];

static void push_btn_evt(uint8_t btn, uint8_t is_long)
{
    uint8_t next = (uint8_t)((s_evt_head + 1U) & 7U);
    if (next != s_evt_tail) {
        s_evt_q[s_evt_head].btn     = btn;
        s_evt_q[s_evt_head].is_long = is_long;
        s_evt_head = next;
    }
}

static bool pop_btn_evt(Button_t *btn, uint8_t *is_long)
{
    if (s_evt_tail == s_evt_head) { return false; }
    *btn     = (Button_t)s_evt_q[s_evt_tail].btn;
    *is_long = s_evt_q[s_evt_tail].is_long;
    s_evt_tail = (uint8_t)((s_evt_tail + 1U) & 7U);
    return true;
}

void Overall_Buttons_Tick1ms(void)
{
    static const uint16_t PINS[4] = {
        BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN
    };
    uint32_t now  = HAL_GetTick();
    uint8_t  mask = 0U;

    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t raw = (HAL_GPIO_ReadPin(BTN_PORT, PINS[i]) == GPIO_PIN_RESET) ? 1U : 0U;
        if (raw) { mask |= (uint8_t)(1U << i); }

        if (raw != s_fbtn[i].last_raw) {
            s_fbtn[i].last_raw   = raw;
            s_fbtn[i].stable_cnt = 0U;
        } else if (raw != s_fbtn[i].stable) {
            if (++s_fbtn[i].stable_cnt >= BTN_DEBOUNCE_MS) {
                s_fbtn[i].stable     = raw;
                s_fbtn[i].stable_cnt = 0U;
                if (raw) {
                    s_fbtn[i].press_ms   = now ? now : 1U;
                    s_fbtn[i].long_fired = 0U;
                    push_btn_evt((uint8_t)(BTN_UP + i), 0U);
                } else {
                    s_fbtn[i].press_ms = 0U;
                }
            }
        }

        if (s_fbtn[i].stable && !s_fbtn[i].long_fired && s_fbtn[i].press_ms != 0U) {
            if ((now - s_fbtn[i].press_ms) >= BTN_LONGPRESS_MS) {
                s_fbtn[i].long_fired = 1U;
                push_btn_evt((uint8_t)(BTN_UP + i), 1U);
            }
        }
    }

    if (mask != g_btn_raw_mask) {
        g_btn_raw_mask = mask;
        if (g_mode == MODE_BUTTONS) {
            s_oled_dirty = 1U;
        }
    }
}

/* Mode 1: Tombol */
static char     s_btn_last_evt[22] = "Tekan tombol...";
static uint32_t s_back_last_ms     = 0;
static uint32_t s_back_hold_t0     = 0;

/* Mode 2: Sensor */
static uint8_t  s_sens_page = 0;    /* 0=Bar, 1=Raw 0..7, 2=Raw 8..15 */
static uint8_t  s_sens_hold = 0;
static uint16_t s_disp_raw[SENSOR_COUNT];
static int16_t  s_disp_pos      = 0;
static uint8_t  s_disp_line_vis = 0;
static uint32_t s_disp_scan_us  = 0;

/* Mode 3: Motor */
typedef enum {
    MP_SET = 0,
    MP_BRAKE,
    MP_WALK
} MotorPhaseKind_t;

typedef struct {
    const char      *name;
    MotorPhaseKind_t kind;
    int16_t          l;
    int16_t          r;
    uint8_t          ch;
} MotorPhase_t;

static const MotorPhase_t MOTOR_PHASES[] = {
    { "KIRI Maju Pelan",   MP_SET,    350,    0, 0 },
    { "KIRI Mundur",       MP_SET,   -350,    0, 0 },
    { "KANAN Maju Pelan",  MP_SET,      0,  350, 0 },
    { "KANAN Mundur",      MP_SET,      0, -350, 0 },
    { "MAJU Lurus Pelan",  MP_SET,    400,  400, 0 },
    { "MAJU Lurus Cepat",  MP_SET,    750,  750, 0 },
    { "MUNDUR Lurus",      MP_SET,   -400, -400, 0 },
    { "PUTAR Kiri",        MP_SET,   -400,  400, 0 },
    { "PUTAR Kanan",       MP_SET,    400, -400, 0 },
    { "REM Aktif (Brake)", MP_BRAKE,    0,    0, 0 },
    { "PIN D0 (PA8/L+)",   MP_WALK,     0,    0, 0 },
    { "PIN D1 (PA9/L-)",   MP_WALK,     0,    0, 1 },
    { "PIN D2 (PA10/R+)",  MP_WALK,     0,    0, 2 },
    { "PIN D3 (PA11/R-)",  MP_WALK,     0,    0, 3 },
};
#define MOTOR_PHASE_COUNT ((uint8_t)(sizeof(MOTOR_PHASES) / sizeof(MOTOR_PHASES[0])))

/* Mode 4: OLED & Power */
static uint8_t s_pwr_page = 0;      /* 0=Power/I2C, 1=Checker, 2=Full White */

/* ============================================================
 * Helper aksi motor aman
 * ============================================================ */
static void motor_disarm_safe(void)
{
    g_motor_armed = 0U;
    Motor_Coast();
    g_cmd_left  = 0;
    g_cmd_right = 0;
}

static void motor_apply_current(void)
{
    if (!g_motor_armed) {
        Motor_Coast();
        g_cmd_left  = 0;
        g_cmd_right = 0;
        return;
    }

    const MotorPhase_t *p = &MOTOR_PHASES[g_motor_phase];
    switch (p->kind) {
    case MP_BRAKE:
        Motor_Brake();
        g_cmd_left  = 0;
        g_cmd_right = 0;
        break;

    case MP_WALK: {
        uint32_t c1 = (p->ch == 0U) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t c2 = (p->ch == 1U) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t c3 = (p->ch == 2U) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t c4 = (p->ch == 3U) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, c1);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, c2);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, c3);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, c4);
        g_cmd_left  = (p->ch == 0U) ?  MOTOR_CMD_MAX : (p->ch == 1U) ? -MOTOR_CMD_MAX : 0;
        g_cmd_right = (p->ch == 2U) ?  MOTOR_CMD_MAX : (p->ch == 3U) ? -MOTOR_CMD_MAX : 0;
        break;
    }

    case MP_SET:
    default:
        Motor_Set(p->l, p->r);
        g_cmd_left  = p->l;
        g_cmd_right = p->r;
        break;
    }
}

static void drain_button_events(void)
{
    s_evt_tail = s_evt_head;
}

static void enter_mode(TestMode_t m)
{
    motor_disarm_safe();
    g_mode = (uint8_t)m;
    s_oled_dirty = 1U;
    drain_button_events();

    if (m == MODE_BUTTONS) {
        snprintf(s_btn_last_evt, sizeof(s_btn_last_evt), "Tekan tombol...");
        s_back_last_ms = 0U;
        s_back_hold_t0 = 0U;
    } else if (m == MODE_SENSOR) {
        s_sens_hold = 0U;
    } else if (m == MODE_MOTOR) {
        g_motor_phase = 0U;
    } else if (m == MODE_OLED_PWR) {
        s_pwr_page = 0U;
    }
}

/* ============================================================
 * Render tiap layar OLED
 * ============================================================ */
static void render_menu(void)
{
    static const char *const ITEMS[MENU_ITEM_COUNT] = {
        "1.Test Tombol",
        "2.Test Sensor 16CH",
        "3.Test Motor ZK5AD",
        "4.Test OLED & Power"
    };
    char buf[24];

    SH1106_Clear();
    SH1106_DrawString(0, 0, "== OVERALL TEST ==");

    for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
        snprintf(buf, sizeof(buf), "%c %s",
                 (i == s_menu_idx) ? '>' : ' ', ITEMS[i]);
        SH1106_DrawString(0, 12 + (int)i * 10, buf);
    }

    SH1106_DrawLine(0, 53, 127, 53, true);
    snprintf(buf, sizeof(buf), "3V3:%4umV  OK:Pilih", (unsigned)g_vdda_mv);
    SH1106_DrawString(0, 56, buf);
    SH1106_Update();
}

static void render_buttons(void)
{
    static const char *const LABELS[4] = { "UP", "DN", "OK", "BK" };
    static const uint16_t PINS[4] = {
        BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN
    };
    char buf[24];

    SH1106_Clear();
    SH1106_DrawString(0, 0, "1. TEST TOMBOL");

    for (int i = 0; i < 4; i++) {
        int x = 4 + i * 31;
        uint8_t down = (HAL_GPIO_ReadPin(BTN_PORT, PINS[i]) == GPIO_PIN_RESET) ? 1U : 0U;
        if (down) {
            SH1106_DrawFillRect(x, 12, 26, 14, true);
            SH1106_DrawString(x + 7, 28, LABELS[i]);
        } else {
            SH1106_DrawRect(x, 12, 26, 14, true);
            SH1106_DrawString(x + 7, 28, LABELS[i]);
        }
    }

    snprintf(buf, sizeof(buf), "U:%2lu D:%2lu O:%2lu B:%2lu",
             (unsigned long)(g_btn_cnt[0] % 100U),
             (unsigned long)(g_btn_cnt[1] % 100U),
             (unsigned long)(g_btn_cnt[2] % 100U),
             (unsigned long)(g_btn_cnt[3] % 100U));
    SH1106_DrawString(0, 38, buf);
    SH1106_DrawString(0, 47, s_btn_last_evt);
    SH1106_DrawString(0, 56, "2x/Hold BACK: Menu");
    SH1106_Update();
}

static void render_sensor(void)
{
    char buf[24];
    SH1106_Clear();

    if (s_sens_page == 0U) {
        snprintf(buf, sizeof(buf), "SENS BAR 1/3 %s", s_sens_hold ? "[HOLD]" : "      ");
        SH1106_DrawString(0, 0, buf);

        /* 16 bar vertikal (0..4095 -> 0..28 px) */
        const int barW = 6, gap = 2, x0 = 1, base = 42;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            int h = (int)((uint32_t)s_disp_raw[i] * 28U / 4095U);
            int x = x0 + i * (barW + gap);
            if (h > 0) {
                SH1106_DrawFillRect(x, base - h, barW, h, true);
            } else {
                SH1106_DrawLine(x, base, x + barW - 1, base, true);
            }
        }

        /* Marker posisi (-7500..+7500 -> x = 4..123) */
        if (s_disp_line_vis) {
            int mx = 64 + ((int)s_disp_pos * 58) / SENSOR_POS_MAX;
            SH1106_DrawLine(mx - 2, 10, mx + 2, 10, true);
            SH1106_DrawLine(mx - 2, 10, mx,     13, true);
            SH1106_DrawLine(mx + 2, 10, mx,     13, true);
        }

        snprintf(buf, sizeof(buf), "pos:%+6d %s",
                 (int)s_disp_pos, s_disp_line_vis ? "LINE" : "LOST");
        SH1106_DrawString(0, 46, buf);

        snprintf(buf, sizeof(buf), "scan:%4luus %4umV",
                 (unsigned long)s_disp_scan_us, (unsigned)g_vdda_mv);
        SH1106_DrawString(0, 56, buf);
    } else {
        uint8_t base_ch = (s_sens_page == 1U) ? 0U : 8U;
        snprintf(buf, sizeof(buf), "RAW CH%02u-%02u %u/3 %s",
                 (unsigned)base_ch, (unsigned)(base_ch + 7U),
                 (unsigned)(s_sens_page + 1U),
                 s_sens_hold ? "H" : " ");
        SH1106_DrawString(0, 0, buf);

        for (uint8_t r = 0; r < 4U; r++) {
            uint8_t c0 = base_ch + r * 2U;
            uint8_t c1 = c0 + 1U;
            snprintf(buf, sizeof(buf), "C%02u:%4u  C%02u:%4u",
                     (unsigned)c0, (unsigned)s_disp_raw[c0],
                     (unsigned)c1, (unsigned)s_disp_raw[c1]);
            SH1106_DrawString(0, 12 + (int)r * 10, buf);
        }
        SH1106_DrawString(0, 56, "U/D:Hal OK:Hold BK:Ex");
    }

    SH1106_Update();
}

static void draw_pwm_bar(int x, int y, int w, int h, int16_t cmd)
{
    int mid = x + w / 2;
    if (cmd >  MOTOR_CMD_MAX) { cmd =  MOTOR_CMD_MAX; }
    if (cmd < -MOTOR_CMD_MAX) { cmd = -MOTOR_CMD_MAX; }

    SH1106_DrawRect(x, y, w, h, true);
    SH1106_DrawLine(mid, y, mid, y + h - 1, true);

    int len = (int)cmd * (w / 2) / MOTOR_CMD_MAX;
    if (len > 0) {
        SH1106_DrawFillRect(mid, y + 1, len, h - 2, true);
    } else if (len < 0) {
        SH1106_DrawFillRect(mid + len, y + 1, -len, h - 2, true);
    }
}

static void render_motor(void)
{
    char buf[24];
    const MotorPhase_t *p = &MOTOR_PHASES[g_motor_phase];

    SH1106_Clear();
    snprintf(buf, sizeof(buf), "MOTOR %02u/%02u  %s",
             (unsigned)(g_motor_phase + 1U),
             (unsigned)MOTOR_PHASE_COUNT,
             g_motor_armed ? "[RUN!]" : "[STOP]");
    SH1106_DrawString(0, 0, buf);

    SH1106_DrawString(0, 12, p->name);

    snprintf(buf, sizeof(buf), "L%+5d R%+5d %4umV",
             (int)g_cmd_left, (int)g_cmd_right, (unsigned)g_vdda_mv);
    SH1106_DrawString(0, 24, buf);

    draw_pwm_bar(2,  36, 60, 12, g_cmd_left);
    draw_pwm_bar(66, 36, 60, 12, g_cmd_right);

    SH1106_DrawString(0, 54, "U/D:Pilih OK:Run/Stp");
    SH1106_Update();
}

static void render_oled_pwr(void)
{
    char buf[24];
    SH1106_Clear();

    if (s_pwr_page == 0U) {
        uint16_t ripple = (g_vdda_max_mv >= g_vdda_min_mv)
                        ? (uint16_t)(g_vdda_max_mv - g_vdda_min_mv) : 0U;
        SH1106_DrawString(0, 0, "4. OLED & POWER 1/3");
        snprintf(buf, sizeof(buf), "Vdda : %4u mV", (unsigned)g_vdda_mv);
        SH1106_DrawString(0, 11, buf);
        snprintf(buf, sizeof(buf), "Min:%4u  Max:%4u",
                 (unsigned)((g_vdda_min_mv == 0xFFFFU) ? 0U : g_vdda_min_mv),
                 (unsigned)g_vdda_max_mv);
        SH1106_DrawString(0, 20, buf);
        snprintf(buf, sizeof(buf), "Rip:%3umV Addr:0x%02X",
                 (unsigned)ripple, (unsigned)g_oled_addr);
        SH1106_DrawString(0, 29, buf);
        snprintf(buf, sizeof(buf), "I2C OK:%lu Err:%lu",
                 (unsigned long)g_sh1106_ok, (unsigned long)g_sh1106_fail);
        SH1106_DrawString(0, 38, buf);
        snprintf(buf, sizeof(buf), "Rec:%lu Up:%lus",
                 (unsigned long)g_sh1106_recover,
                 (unsigned long)(HAL_GetTick() / 1000U));
        SH1106_DrawString(0, 47, buf);
        SH1106_DrawString(0, 56, "OK:RstMinMax U/D:Hal");
    } else if (s_pwr_page == 1U) {
        /* Checkerboard 8x8 */
        for (int y = 0; y < SH1106_HEIGHT; y += 8) {
            for (int x = 0; x < SH1106_WIDTH; x += 8) {
                if (((x / 8) + (y / 8)) & 1) {
                    SH1106_DrawFillRect(x, y, 8, 8, true);
                }
            }
        }
        SH1106_DrawFillRect(10, 24, 108, 16, false);
        SH1106_DrawRect(10, 24, 108, 16, true);
        SH1106_DrawString(14, 28, "CHECKER 2/3 BK:Ex");
    } else {
        /* Full white stress test */
        SH1106_DrawFillRect(0, 0, SH1106_WIDTH, SH1106_HEIGHT, true);
    }

    SH1106_Update();
}

/* ============================================================
 * Proses input per mode
 * ============================================================ */
static void handle_input(void)
{
    Button_t evt_btn;
    uint8_t  is_long;

    while (pop_btn_evt(&evt_btn, &is_long)) {
        Button_t press  = (!is_long) ? evt_btn : BTN_NONE;
        Button_t lpress = (is_long)  ? evt_btn : BTN_NONE;
        s_oled_dirty = 1U;

        switch ((TestMode_t)g_mode) {
        case MODE_MENU:
            if (press == BTN_UP) {
                s_menu_idx = (s_menu_idx == 0U) ? (MENU_ITEM_COUNT - 1U) : (uint8_t)(s_menu_idx - 1U);
            } else if (press == BTN_DOWN) {
                s_menu_idx = (uint8_t)((s_menu_idx + 1U) % MENU_ITEM_COUNT);
            } else if (press == BTN_OK) {
                enter_mode((TestMode_t)(MODE_BUTTONS + s_menu_idx));
                return;
            }
            break;

        case MODE_BUTTONS: {
            uint32_t now = HAL_GetTick();
            if (press != BTN_NONE) {
                int idx = (int)press - (int)BTN_UP;
                static const char *const BNAMES[4] = { "UP", "DOWN", "OK", "BACK" };
                if (idx >= 0 && idx < 4) {
                    g_btn_cnt[idx]++;
                    snprintf(s_btn_last_evt, sizeof(s_btn_last_evt), "Last: %s SHORT", BNAMES[idx]);
                }
                if (press == BTN_BACK) {
                    if (s_back_last_ms != 0U && (now - s_back_last_ms) <= 350U) {
                        enter_mode(MODE_MENU);
                        return;
                    }
                    s_back_last_ms = now;
                }
            }
            if (lpress != BTN_NONE) {
                if (lpress == BTN_BACK) {
                    enter_mode(MODE_MENU);
                    return;
                } else {
                    int idx = (int)lpress - (int)BTN_UP;
                    static const char *const BNAMES[4] = { "UP", "DOWN", "OK", "BACK" };
                    if (idx >= 0 && idx < 4) {
                        snprintf(s_btn_last_evt, sizeof(s_btn_last_evt), "Last: %s LONG", BNAMES[idx]);
                    }
                }
            }
            break;
        }

        case MODE_SENSOR:
            if (press == BTN_UP) {
                s_sens_page = (s_sens_page == 0U) ? 2U : (uint8_t)(s_sens_page - 1U);
            } else if (press == BTN_DOWN) {
                s_sens_page = (uint8_t)((s_sens_page + 1U) % 3U);
            } else if (press == BTN_OK) {
                s_sens_hold = (uint8_t)!s_sens_hold;
            } else if (press == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_MOTOR:
            if (press == BTN_UP) {
                motor_disarm_safe();
                g_motor_phase = (g_motor_phase == 0U) ? (MOTOR_PHASE_COUNT - 1U) : (uint8_t)(g_motor_phase - 1U);
            } else if (press == BTN_DOWN) {
                motor_disarm_safe();
                g_motor_phase = (uint8_t)((g_motor_phase + 1U) % MOTOR_PHASE_COUNT);
            } else if (press == BTN_OK) {
                g_motor_armed = (uint8_t)!g_motor_armed;
                motor_apply_current();
            } else if (press == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_OLED_PWR:
            if (press == BTN_UP) {
                s_pwr_page = (s_pwr_page == 0U) ? 2U : (uint8_t)(s_pwr_page - 1U);
            } else if (press == BTN_DOWN) {
                s_pwr_page = (uint8_t)((s_pwr_page + 1U) % 3U);
            } else if (press == BTN_OK) {
                if (s_pwr_page == 0U) {
                    g_vdda_min_mv = g_vdda_mv;
                    g_vdda_max_mv = g_vdda_mv;
                } else {
                    s_pwr_page = 0U;
                }
            } else if (press == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;
        }
    }

    /* Cek hold BACK >= 400 ms saat di MODE_BUTTONS walaupun queue sedang kosong */
    if (g_mode == MODE_BUTTONS) {
        uint32_t now = HAL_GetTick();
        if (g_btn_raw_mask & (1U << 3)) {
            if (s_back_hold_t0 == 0U) {
                s_back_hold_t0 = now ? now : 1U;
            } else if ((now - s_back_hold_t0) >= 400U) {
                enter_mode(MODE_MENU);
            }
        } else {
            s_back_hold_t0 = 0U;
        }
    }
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    dwt_init();

    MX_GPIO_Init();
    MX_TIM1_Init();

    /* Pastikan motor dalam keadaan COAST (diam) sesegera mungkin saat boot */
    Motor_Init();
    __HAL_TIM_MOE_ENABLE(&htim1);
    Motor_Coast();

    /* Jeda stabilisasi power dari buck converter sebelum cek pin & init OLED I2C */
    HAL_Delay(BOOT_SETTLE_MS);

    i2c_pin_diag();
    MX_I2C1_Init();
    MX_ADC1_Init();
    Buttons_Init();
    g_btn_ready = 1U;

    oled_init_safe();
    sample_vdda_only();
    draw_boot_splash();
    HAL_Delay(BOOT_SPLASH_MS);

    enter_mode(MODE_MENU);

    uint32_t last_oled       = 0;
    uint32_t last_oled_retry = 0;
    uint32_t last_vdda       = 0;
    uint32_t last_led        = 0;

    for (;;) {
        uint32_t now = HAL_GetTick();
        g_loop++;

        handle_input();

        /* Scan sensor penuh saat di MODE_SENSOR, atau baca Vdda periodik di mode lain */
        if (g_mode == MODE_SENSOR) {
            sensor_scan_full();
            if (!s_sens_hold) {
                for (int i = 0; i < SENSOR_COUNT; i++) {
                    s_disp_raw[i] = g_raw[i];
                }
                s_disp_pos      = g_pos;
                s_disp_line_vis = g_line_vis;
                s_disp_scan_us  = g_scan_us;
            }
        } else if ((now - last_vdda) >= VDDA_SAMPLE_MS) {
            last_vdda = now;
            sample_vdda_only();
        }

        /* Heartbeat PC13: cepat (100 ms) jika motor aktif, normal (500 ms) saat standby */
        uint32_t led_period = (g_mode == MODE_MOTOR && g_motor_armed) ? 100U : 500U;
        if ((now - last_led) >= led_period) {
            last_led = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        /* Jika OLED belum terdeteksi saat boot, coba ulang tiap 500 ms (hot-plug) */
        if (!g_oled_ok && (now - last_oled_retry) >= 500U) {
            last_oled_retry = now;
            oled_init_safe();
            if (g_oled_ok) {
                s_oled_dirty = 1U;
            }
        }

        /* Refresh OLED segera saat ada input, atau periodik setelah frame selesai.
         * Di menu/tombol/motor/power pakai 400 ms agar bus I2C tidak jenuh terus-menerus. */
        uint32_t bg_period = (g_mode == MODE_SENSOR) ? OLED_SENSOR_MS : OLED_BG_REFRESH_MS;
        if (g_oled_ok && (s_oled_dirty || (now - last_oled) >= bg_period)) {
            s_oled_dirty = 0U;
            switch ((TestMode_t)g_mode) {
            case MODE_MENU:     render_menu();     break;
            case MODE_BUTTONS:  render_buttons();  break;
            case MODE_SENSOR:   render_sensor();   break;
            case MODE_MOTOR:    render_motor();    break;
            case MODE_OLED_PWR: render_oled_pwr(); break;
            }
            last_oled = HAL_GetTick();
        }

        HAL_Delay(1);
    }
}
