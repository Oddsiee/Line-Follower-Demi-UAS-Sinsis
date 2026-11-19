#include "sensor.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * sensor.c — Scan 16 kanal Dariyan-X via ADC1 + DMA2 Stream0
 * ============================================================ */

extern ADC_HandleTypeDef hadc1;

volatile uint32_t g_sensor_scan_us = 0U;
volatile uint32_t g_sensor_adc_err = 0U;

static uint16_t         s_raw[SENSOR_COUNT];
static uint16_t         s_adc[SENSOR_GROUPS];
static volatile uint8_t s_adc_done   = 0U;
static int16_t          s_last_pos   = 0;
static bool             s_line_vis   = false;

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t mhz   = SystemCoreClock / 1000000U;
    if (mhz < 1U) {
        mhz = 1U;
    }
    uint32_t ticks = us * mhz;
    while ((DWT->CYCCNT - start) < ticks) { }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) {
        s_adc_done = 1U;
    }
}

static void mux_set(uint8_t state)
{
    HAL_GPIO_WritePin(MUX_IN1_PORT, MUX_IN1_PIN,
                      (state & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_IN2_PORT, MUX_IN2_PIN,
                      (state & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool adc_read4(uint16_t out[SENSOR_GROUPS])
{
    s_adc_done = 0U;
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc, SENSOR_GROUPS) != HAL_OK) {
        return false;
    }

    uint32_t t0 = HAL_GetTick();
    while (!s_adc_done) {
        if ((HAL_GetTick() - t0) > 10U) {
            HAL_ADC_Stop_DMA(&hadc1);
            return false;
        }
    }
    HAL_ADC_Stop_DMA(&hadc1);

    for (int i = 0; i < SENSOR_GROUPS; i++) {
        out[i] = s_adc[i];
    }
    return true;
}

void Sensor_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    GPIO_InitTypeDef g = {0};

    dwt_init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Pastikan pin selektor mux PA3 (In1) & PB0 (In2) sebagai output push-pull */
    HAL_GPIO_WritePin(MUX_IN1_PORT, MUX_IN1_PIN, GPIO_PIN_RESET);
    g.Pin   = MUX_IN1_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MUX_IN1_PORT, &g);

    HAL_GPIO_WritePin(MUX_IN2_PORT, MUX_IN2_PIN, GPIO_PIN_RESET);
    g.Pin   = MUX_IN2_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MUX_IN2_PORT, &g);

    /* Pastikan Rank 1..4 ADC1 memakai kanal 0, 1, 2, 9 (PA0, PA1, PA2, PB1) */
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    ch.Offset       = 0;
    ch.Channel = SENS_ADC_CH_OUT1; ch.Rank = 1; HAL_ADC_ConfigChannel(&hadc1, &ch);
    ch.Channel = SENS_ADC_CH_OUT2; ch.Rank = 2; HAL_ADC_ConfigChannel(&hadc1, &ch);
    ch.Channel = SENS_ADC_CH_OUT3; ch.Rank = 3; HAL_ADC_ConfigChannel(&hadc1, &ch);
    ch.Channel = SENS_ADC_CH_OUT4; ch.Rank = 4; HAL_ADC_ConfigChannel(&hadc1, &ch);

    mux_set(0U);
    for (int i = 0; i < SENSOR_COUNT; i++) {
        s_raw[i] = 0U;
    }
    g_sensor_adc_err = 0U;
    g_sensor_scan_us = 0U;
    s_last_pos       = 0;
    s_line_vis       = false;
}

/* Pemetaan indeks logis kiri->kanan (0..15) ke indeks raw (group*4 + mux_state).
 * Hasil uji live (kiri=putih, kanan=hitam):
 *   G3 (Out4/PB1: 12..15) & G0 (Out1/PA0: 0..3)  = sisi KIRI
 *   G1 (Out2/PA1: 4..7)   & G2 (Out3/PA2: 8..11) = sisi KANAN */
static const uint8_t SENSOR_MAP[SENSOR_COUNT] = {
    12, 13, 14, 15,   /* Kiri luar   (Out4 / PB1) */
     0,  1,  2,  3,   /* Kiri tengah (Out1 / PA0) */
     4,  5,  6,  7,   /* Kanan tengah(Out2 / PA1) */
     8,  9, 10, 11    /* Kanan luar  (Out3 / PA2) */
};

void Sensor_ReadRaw(uint16_t raw[SENSOR_COUNT])
{
    uint32_t t0 = DWT->CYCCNT;
    uint16_t hw_raw[SENSOR_COUNT];

    for (uint8_t s = 0U; s < MUX_STATES; s++) {
        mux_set(s);
        delay_us(MUX_SETTLE_US);

        uint32_t acc[SENSOR_GROUPS] = { 0U, 0U, 0U, 0U };
        uint32_t ok_cnt = 0U;

        for (uint32_t k = 0U; k < SENSOR_OVERSAMPLE; k++) {
            uint16_t v[SENSOR_GROUPS];
            if (adc_read4(v)) {
                for (int g = 0; g < SENSOR_GROUPS; g++) {
                    acc[g] += v[g];
                }
                ok_cnt++;
            }
        }

        if (ok_cnt == 0U) {
            g_sensor_adc_err++;
            ok_cnt = 1U;
        }

        for (int g = 0; g < SENSOR_GROUPS; g++) {
            hw_raw[g * MUX_STATES + s] = (uint16_t)(acc[g] / ok_cnt);
        }
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        s_raw[i] = hw_raw[SENSOR_MAP[i]];
    }

    uint32_t mhz = SystemCoreClock / 1000000U;
    if (mhz < 1U) {
        mhz = 1U;
    }
    g_sensor_scan_us = (DWT->CYCCNT - t0) / mhz;

    if (raw != NULL) {
        for (int i = 0; i < SENSOR_COUNT; i++) {
            raw[i] = s_raw[i];
        }
    }
}

int16_t Sensor_ComputePosition(const uint16_t norm[SENSOR_COUNT], bool *line_visible)
{
    int32_t sum_w  = 0;
    int32_t sum_iw = 0;
    bool    vis    = false;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        uint16_t val = norm[i];
        if (val >= (uint16_t)SENSOR_ON_LINE_MIN) {
            sum_w  += (int32_t)val;
            sum_iw += (int32_t)val * i;
            vis     = true;
        }
    }

    s_line_vis = vis;
    if (line_visible != NULL) {
        *line_visible = vis;
    }

    if (vis && sum_w > 0) {
        /* Indeks 0..15 -> -7500..+7500 (tengah 7.5 = 0) */
        int32_t pos = ((sum_iw * SENSOR_POS_SCALE) / sum_w) - SENSOR_POS_MAX;
        if (pos > SENSOR_POS_MAX)  { pos =  SENSOR_POS_MAX; }
        if (pos < -SENSOR_POS_MAX) { pos = -SENSOR_POS_MAX; }
        s_last_pos = (int16_t)pos;
        return s_last_pos;
    }

    /* Garis hilang: ingat arah terakhir (M5.4) */
    if (s_last_pos > 0) {
        return SENSOR_POS_MAX;
    }
    if (s_last_pos < 0) {
        return -SENSOR_POS_MAX;
    }
    return 0;
}

int16_t Sensor_GetPosition(void)
{
    return s_last_pos;
}

bool Sensor_LineVisible(void)
{
    return s_line_vis;
}

uint32_t Sensor_GetLastScanUs(void)
{
    return g_sensor_scan_us;
}

uint32_t Sensor_GetAdcErr(void)
{
    return g_sensor_adc_err;
}
