#include "sensor.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * sensor.c — scan 16 kanal Dariyan-X (dual 4-ch mux)
 *
 * Pin (lihat config.h):
 *   Out1..Out4 = PA0/PA1/PA2/PB1 -> ADC1_IN0/IN1/IN2/IN9
 *   In1 = PA3 (mux bit0), In2 = PB0 (mux bit1)
 *
 * Handle ADC/DMA disediakan oleh main.c (CubeMX):
 *   extern ADC_HandleTypeDef hadc1;
 * ============================================================ */

extern ADC_HandleTypeDef hadc1;

static uint16_t s_raw[SENSOR_COUNT];
static uint16_t s_adc[4];
static volatile uint8_t s_adc_done;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) s_adc_done = 1;
}

/* Set kombinasi (In2, In1) = state 0..3. Dua port ditulis terpisah. */
static void mux_set(uint8_t state)
{
    HAL_GPIO_WritePin(MUX_IN1_PORT, MUX_IN1_PIN,
                      (state & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_IN2_PORT, MUX_IN2_PIN,
                      (state & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
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

void Sensor_Init(void)
{
    /* GPIO mux + ADC sudah diinisialisasi CubeMX (MX_GPIO_Init/MX_ADC1_Init).
     * Pastikan mux mulai dari state 0. */
    mux_set(0);
    for (int i = 0; i < SENSOR_COUNT; i++) s_raw[i] = 0;
}

void Sensor_ReadRaw(uint16_t raw[SENSOR_COUNT])
{
    for (uint8_t s = 0; s < MUX_STATES; s++) {
        mux_set(s);
        /* settling mux */
        for (volatile uint32_t d = 0; d < (MUX_SETTLE_US * 16U); d++) { }

        uint16_t v[4];
        if (adc_read4(v)) {
            for (int g = 0; g < SENSOR_GROUPS; g++) {
                s_raw[g * MUX_STATES + s] = v[g];
            }
        }
    }

    for (int i = 0; i < SENSOR_COUNT; i++) raw[i] = s_raw[i];
}

int16_t Sensor_GetPosition(void)
{
    /* Weighted average posisi sensor yang melihat garis.
     * TODO (M5): pakai nilai hasil kalibrasi (Calib_Normalize). */
    int32_t sum_w = 0, sum_iw = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (s_raw[i] >= SENSOR_ON_LINE_MIN) {
            sum_w  += s_raw[i];
            sum_iw += (int32_t)s_raw[i] * i;
        }
    }
    if (sum_w == 0) return 0;

    /* posisi 0..15 -> -7500..+7500 (tengah = 7.5) */
    int32_t idx_x10 = (sum_iw * 10) / sum_w;          /* 0..150 */
    return (int16_t)((idx_x10 - 75) * (SENSOR_POS_SCALE / 10));
}

bool Sensor_LineVisible(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (s_raw[i] >= SENSOR_ON_LINE_MIN) return true;
    }
    return false;
}
