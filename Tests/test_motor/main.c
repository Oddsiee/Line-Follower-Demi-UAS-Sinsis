/**
 * main.c — Test DRIVER MOTOR ZK-5AD (standalone, tanpa CubeIDE)
 *
 * Tujuan: memastikan koneksi STM32 -> driver motor ZK-5AD sehat, sebelum
 * masuk ke Milestone 3 (motor.c) dan tuning PID.
 *
 * Wiring (sesuai pinout master):
 *   D0 = PA8  (TIM1_CH1)  Motor kiri  IN1
 *   D1 = PA9  (TIM1_CH2)  Motor kiri  IN2
 *   D2 = PA10 (TIM1_CH3)  Motor kanan IN1
 *   D3 = PA11 (TIM1_CH4)  Motor kanan IN2
 *   GND driver -> GND board (WAJIB), pin 3V3 & VT driver = NC (JANGAN disambung)
 *
 * OLED (opsional, PB6=SCL / PB7=SDA) menampilkan fase yang sedang berjalan.
 *
 * Logika H-bridge (ZK-5AD):
 *   Maju   : IN1 = PWM, IN2 = LOW
 *   Mundur : IN1 = LOW, IN2 = PWM
 *   Rem    : IN1 = HIGH, IN2 = HIGH
 *   Coast  : IN1 = LOW,  IN2 = LOW
 *
 * Build : .\build.ps1
 * Flash : .\flash.ps1
 * Baca  : .\read_motor.ps1   (baca g_* + register TIM1 via SWD, tanpa reset)
 *
 * PERINGATAN: motor akan berputar. Angkat/ganjal dulu kedua roda robot.
 */

#include "stm32f4xx_hal.h"
#include "config.h"
#include "motor.h"
#include "sh1106.h"
#include <stdio.h>

/* ============================================================
 * Handle global — dipakai motor.c (htim1) & sh1106.c (hi2c1) lewat extern
 * ============================================================ */
TIM_HandleTypeDef htim1;
I2C_HandleTypeDef hi2c1;

/* ============================================================
 * Variabel uji — dibaca via SWD oleh read_motor.ps1
 * ============================================================ */
volatile uint32_t g_phase       = 0;    /* indeks fase sekarang         */
volatile uint32_t g_phase_count = 0;    /* jumlah fase total            */
volatile uint32_t g_loop        = 0;    /* penghitung loop              */
volatile int16_t  g_cmd_left    = 0;    /* perintah motor kiri          */
volatile int16_t  g_cmd_right   = 0;    /* perintah motor kanan         */
volatile uint8_t  g_oled_ok     = 0;    /* 1 kalau OLED terdeteksi      */
volatile uint8_t  g_paused      = 0;    /* 1 kalau auto-advance dijeda  */
volatile uint8_t  g_btn_ok      = 0;    /* status tombol OK  (PA5)      */
volatile uint8_t  g_btn_back    = 0;    /* status tombol BACK(PA4)      */
volatile uint8_t  g_moe_on      = 0;    /* TIM1 BDTR MOE aktif?         */

void Error_Handler(void);

/* ============================================================
 * Fase uji
 * ============================================================ */
typedef enum {
    PH_COAST = 0,
    PH_WALK,        /* satu kanal D0..D3 saja, 100% duty (uji koneksi) */
    PH_SET,         /* Motor_Set(l, r) */
    PH_BRAKE        /* kedua pin HIGH */
} ph_kind_t;

typedef struct {
    const char *name;
    ph_kind_t   kind;
    int16_t     l;      /* perintah kiri  (PH_SET)  */
    int16_t     r;      /* perintah kanan (PH_SET)  */
    uint8_t     ch;     /* kanal 0..3     (PH_WALK) */
} phase_t;

/* Fase 1..4 = "pin walk": satu per satu kanal diberi 100% duty.
 * Inilah uji koneksi STM32->driver yang paling langsung: ukur D0..D3
 * pakai multimeter (atau lihat motor menyentak) satu per satu. */
static const phase_t PHASES[] = {
    { "COAST bebas",   PH_COAST,    0,    0, 0 },
    { "PIN D0 kiri+",  PH_WALK,     0,    0, 0 },
    { "PIN D1 kiri-",  PH_WALK,     0,    0, 1 },
    { "PIN D2 kanan+", PH_WALK,     0,    0, 2 },
    { "PIN D3 kanan-", PH_WALK,     0,    0, 3 },
    { "MAJU pelan",    PH_SET,    300,  300, 0 },
    { "MAJU cepat",    PH_SET,    700,  700, 0 },
    { "KIRI saja",     PH_SET,    600,    0, 0 },
    { "KANAN saja",    PH_SET,      0,  600, 0 },
    { "MUNDUR pelan",  PH_SET,   -300, -300, 0 },
    { "REM (brake)",   PH_BRAKE,    0,    0, 0 },
};
#define PHASE_N ((uint32_t)(sizeof(PHASES) / sizeof(PHASES[0])))

#define PHASE_MS  1500U   /* lama satu fase otomatis (ms) */
#define OLED_MS    200U   /* periode refresh OLED    (ms) */

static uint32_t s_phase_t0 = 0;
static uint32_t s_oled_t0  = 0;
static uint8_t  s_oled_dirty = 1;

/* ============================================================
 * Clock: HSI 16 MHz (default, tanpa PLL)
 *   TIM1 di APB2 div1 -> clock timer = 16 MHz
 *   f_PWM = 16 MHz / (ARR+1) = 16 MHz / 1000 = 16 kHz
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
 * GPIO
 *   PA8..PA11 -> AF1 (TIM1_CH1..CH4)
 *   PB6/PB7   -> AF4 (I2C1) untuk OLED opsional
 *   PA4..PA7  -> input pull-up (tombol BACK/OK/DOWN/UP)
 *   PC13      -> LED onboard (active low)
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA8..PA11 = TIM1_CH1..CH4, AF1, push-pull */
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &g);

    /* PB6/PB7 = I2C1 (OLED opsional) */
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    /* PA4..PA7 = tombol, input pull-up (aktif LOW) */
    g.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOA, &g);

    /* PC13 = LED onboard (active low) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_13;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

/* ============================================================
 * I2C1: PB6/PB7, 400 kHz (OLED)
 * ============================================================ */
static void MX_I2C1_Init(void)
{
    /* WAJIB: tanpa clock enable, register I2C1 tidak bisa diakses */
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
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
}

/* ============================================================
 * TIM1: PWM 4 kanal untuk driver ZK-5AD
 *   PSC = 0        -> timer clock 16 MHz
 *   ARR = 999      -> 16 kHz
 *   compare 0..999 = duty 0..100% (== Motor_Set -1000..+1000 / 1000)
 * ============================================================ */
static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};
    TIM_OC_InitTypeDef      sConfigOC          = {0};

    __HAL_RCC_TIM1_CLK_ENABLE();

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = MOTOR_PWM_ARR;   /* 999 -> 16 kHz */
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

void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}

/* ============================================================
 * OLED — hanya kalau terdeteksi (PB6/PB7)
 * ============================================================ */
static void oled_detect(void)
{
    bool a3c = SH1106_Probe(0x3C);
    bool a3d = SH1106_Probe(0x3D);

    if (a3c || a3d) {
        g_oled_ok = 1;
        SH1106_SetAddress(a3c ? 0x3C : 0x3D);
        SH1106_Init();
    } else {
        g_oled_ok = 0;
    }
}

static void oled_bar(int x, int y, int w, int h, int16_t cmd)
{
    int mid = x + w / 2;
    int len;

    if (cmd >  MOTOR_CMD_MAX) cmd =  MOTOR_CMD_MAX;
    if (cmd < -MOTOR_CMD_MAX) cmd = -MOTOR_CMD_MAX;

    SH1106_DrawRect(x, y, w, h, true);
    SH1106_DrawLine(mid, y, mid, y + h - 1, true);

    len = (int)cmd * (w / 2) / MOTOR_CMD_MAX;
    if (len > 0) {
        SH1106_DrawFillRect(mid, y + 1, len, h - 2, true);
    } else if (len < 0) {
        SH1106_DrawFillRect(mid + len, y + 1, -len, h - 2, true);
    }
}

static void oled_render(void)
{
    char buf[24];

    SH1106_Clear();
    SH1106_DrawString(0, 0, "MOTOR DRIVER TEST");

    snprintf(buf, sizeof(buf), "F%02u/%02u %s",
             (unsigned)(g_phase + 1U), (unsigned)PHASE_N,
             g_paused ? "PAUSE" : "AUTO");
    SH1106_DrawString(0, 12, buf);

    SH1106_DrawString(0, 24, PHASES[g_phase].name);

    snprintf(buf, sizeof(buf), "L%+5d  R%+5d", (int)g_cmd_left, (int)g_cmd_right);
    SH1106_DrawString(0, 36, buf);

    oled_bar(2,  48, 60, 12, g_cmd_left);
    oled_bar(66, 48, 60, 12, g_cmd_right);

    SH1106_Update();
}

/* ============================================================
 * Mesin uji
 * ============================================================ */
static void apply_phase(void)
{
    const phase_t *p = &PHASES[g_phase];

    switch (p->kind) {
    case PH_COAST:
        Motor_Coast();
        g_cmd_left = 0;
        g_cmd_right = 0;
        break;

    case PH_BRAKE:
        Motor_Brake();
        g_cmd_left = 0;
        g_cmd_right = 0;
        break;

    case PH_WALK: {
        /* Nyalakan SATU kanal saja 100% duty, sisanya 0.
         * Ch0=D0(PA8/CH1), Ch1=D1(PA9/CH2), Ch2=D2(PA10/CH3), Ch3=D3(PA11/CH4). */
        uint32_t ch1 = (p->ch == 0) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t ch2 = (p->ch == 1) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t ch3 = (p->ch == 2) ? (uint32_t)MOTOR_PWM_ARR : 0U;
        uint32_t ch4 = (p->ch == 3) ? (uint32_t)MOTOR_PWM_ARR : 0U;

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ch1);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ch2);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ch3);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, ch4);

        /* tampilkan sebagai perintah penuh supaya bar OLED jelas */
        g_cmd_left  = (p->ch == 0) ?  MOTOR_CMD_MAX : (p->ch == 1) ? -MOTOR_CMD_MAX : 0;
        g_cmd_right = (p->ch == 2) ?  MOTOR_CMD_MAX : (p->ch == 3) ? -MOTOR_CMD_MAX : 0;
        break;
    }

    case PH_SET:
    default:
        Motor_Set(p->l, p->r);
        g_cmd_left = p->l;
        g_cmd_right = p->r;
        break;
    }

    /* LED onboard: nyala saat fase REM, mati saat lainnya */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      (p->kind == PH_BRAKE) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    s_phase_t0 = HAL_GetTick();
    s_oled_dirty = 1;
}

static void next_phase(void)
{
    g_phase = (g_phase + 1U) % PHASE_N;
    apply_phase();
}

static void button_scan(void)
{
    static uint32_t last_ok = 0, last_back = 0;
    uint32_t now = HAL_GetTick();

    uint8_t ok   = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); /* PA5 */
    uint8_t back = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET); /* PA4 */

    g_btn_ok   = ok;
    g_btn_back = back;

    if (ok && (now - last_ok) > 250U) {
        last_ok = now;
        next_phase();           /* OK = fase berikutnya (manual) */
    }
    if (back && (now - last_back) > 250U) {
        last_back = now;
        g_paused = (uint8_t)!g_paused;      /* BACK = jeda/lanjut auto */
        if (g_paused) {
            Motor_Coast();
            g_cmd_left = 0;
            g_cmd_right = 0;
            s_oled_dirty = 1;
        } else {
            s_phase_t0 = now;
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM1_Init();

    /* OLED opsional */
    oled_detect();

    /* Mulai PWM 4 kanal + pastikan MOE (main output enable) TIM1 aktif.
     * Tanpa MOE, TIM1 (advanced timer) tidak mengeluarkan sinyal di pin. */
    Motor_Init();
    __HAL_TIM_MOE_ENABLE(&htim1);
    g_moe_on = ((TIM1->BDTR & TIM_BDTR_MOE) != 0U) ? 1U : 0U;

    g_phase_count = (uint32_t)PHASE_N;
    g_phase = 0;
    apply_phase();

    for (;;) {
        uint32_t now = HAL_GetTick();

        g_loop++;

        button_scan();

        if (!g_paused && (now - s_phase_t0) >= PHASE_MS) {
            next_phase();
        }

        if (g_oled_ok && (s_oled_dirty || (now - s_oled_t0) >= OLED_MS)) {
            s_oled_t0 = now;
            s_oled_dirty = 0;
            oled_render();
        }

        HAL_Delay(5);
    }
}
