#include "app.h"
#include "buttons.h"
#include "calibration.h"
#include "config.h"
#include "motor.h"
#include "params.h"
#include "sensor.h"
#include "sh1106.h"
#include "storage.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* ============================================================
 * Konstanta waktu & UI
 * ============================================================ */
#define BOOT_SETTLE_MS       150U
#define BOOT_SPLASH_MS       600U
#define OLED_BG_REFRESH_MS   400U
#define OLED_LIVE_MS          80U
#define MENU_ITEM_COUNT        6U
#define MENU_VISIBLE_ROWS      4U
#define SENSOR_TEST_PAGES      4U
#define CALIB_VIEW_PAGES       5U
#define OLED_TEST_PAGES        5U
#define MANUAL_SPD_STEP      100

typedef enum {
    MODE_MENU = 0,
    MODE_SENSOR_TEST,
    MODE_CALIB,
    MODE_MOTOR_PRESET,
    MODE_MOTOR_MANUAL,
    MODE_OLED_TEST,
    MODE_BUTTON_TEST
} TestMode_t;

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
    { "KIRI Maju Pelan",    MP_SET,    350,    0, 0 },
    { "KIRI Maju Cepat",    MP_SET,    750,    0, 0 },
    { "KIRI Mundur",        MP_SET,   -350,    0, 0 },
    { "KANAN Maju Pelan",   MP_SET,      0,  350, 0 },
    { "KANAN Maju Cepat",   MP_SET,      0,  750, 0 },
    { "KANAN Mundur",       MP_SET,      0, -350, 0 },
    { "MAJU Lurus Pelan",   MP_SET,    400,  400, 0 },
    { "MAJU Lurus Cepat",   MP_SET,    750,  750, 0 },
    { "MUNDUR Lurus",       MP_SET,   -400, -400, 0 },
    { "PUTAR Kiri (Spin)",  MP_SET,   -400,  400, 0 },
    { "PUTAR Kanan (Spin)", MP_SET,    400, -400, 0 },
    { "REM Aktif (Brake)",  MP_BRAKE,    0,    0, 0 },
    { "PIN D3 (PA11 Kiri+)", MP_WALK,    0,    0, 3 },
    { "PIN D2 (PA10 Kiri-)", MP_WALK,    0,    0, 2 },
    { "PIN D1 (PA9 Kanan+)", MP_WALK,    0,    0, 1 },
    { "PIN D0 (PA8 Kanan-)", MP_WALK,    0,    0, 0 },
};
#define MOTOR_PHASE_COUNT ((uint8_t)(sizeof(MOTOR_PHASES) / sizeof(MOTOR_PHASES[0])))

typedef struct {
    const char *label;
    uint8_t     target;
    uint16_t    freq_hz;
    bool        slow_decay;
} ManualProfile_t;

static const ManualProfile_t MANUAL_PROFILES[] = {
    { "KIRI+KANAN",  0U, 1000U, false },
    { "KIRI SAJA",   1U, 1000U, false },
    { "KANAN SAJA",  2U, 1000U, false },
    { "PUTAR KIRI",  3U, 1000U, false },
    { "L+R 500Hz",   0U,  500U, false },
    { "L+R 2kHz",    0U, 2000U, false },
    { "L+R 1k SLOW", 0U, 1000U, true  },
};
#define MANUAL_PROFILE_COUNT ((uint8_t)(sizeof(MANUAL_PROFILES) / sizeof(MANUAL_PROFILES[0])))

/* ============================================================
 * State internal & parameter tersimpan
 * ============================================================ */
static Params_t   s_params;
static uint8_t    s_flash_loaded = 0U;

static TestMode_t s_mode         = MODE_MENU;
static uint8_t    s_menu_idx     = 0U;
static uint8_t    s_menu_top     = 0U;
static uint8_t    s_oled_ok      = 0U;
static uint8_t    s_oled_addr    = 0x3C;
static uint8_t    s_oled_dirty   = 1U;

/* State Sensor & Kalibrasi */
static uint8_t    s_sens_page     = 0U; /* 0=Norm+Pos, 1=RawBar, 2=Raw0-7, 3=Raw8-15 */
static uint8_t    s_sens_hold     = 0U;
static uint16_t   s_disp_raw[SENSOR_COUNT];
static uint16_t   s_disp_norm[SENSOR_COUNT];
static int16_t    s_disp_pos      = 0;
static bool       s_disp_line_vis = false;
static uint32_t   s_disp_scan_us  = 0U;

static uint8_t    s_calib_running = 0U;
static uint8_t    s_calib_page    = 0U; /* 0=Info, 1=Min0-7, 2=Max0-7, 3=Min8-15, 4=Max8-15 */

/* State Motor */
static uint8_t    s_motor_armed    = 0U;
static uint8_t    s_motor_phase    = 0U;
static int16_t    s_cmd_left       = 0;
static int16_t    s_cmd_right      = 0;
static int16_t    s_manual_spd     = 400;
static uint8_t    s_manual_target  = 0U;
static uint8_t    s_manual_profile = 0U;

/* State Test OLED */
static uint8_t    s_oled_page        = 0U;
static uint8_t    s_oled_checker_inv = 0U;
static uint8_t    s_oled_frame_mode  = 0U;
static int16_t    s_anim_x10         = 75;
static int16_t    s_anim_dir         = 1;
static uint32_t   s_last_frame_ms    = 0U;

/* State Test Tombol */
static uint32_t   s_btn_cnt[4]        = { 0U, 0U, 0U, 0U };
static char       s_btn_last_evt[22]  = "Tekan tombol...";
static uint8_t    s_prev_raw_mask     = 0U;
static uint32_t   s_back_last_ms      = 0U;
static uint32_t   s_back_hold_t0      = 0U;

/* Timer loop */
static uint32_t   s_last_oled       = 0U;
static uint32_t   s_last_oled_retry = 0U;
static uint32_t   s_last_led        = 0U;

/* ============================================================
 * Helper OLED, Flash, & Motor
 * ============================================================ */
static void oled_init_safe(bool force_init)
{
    bool a3c = SH1106_Probe(0x3C);
    bool a3d = a3c ? false : SH1106_Probe(0x3D);
    g_sh1106_scan_count = (a3c ? 1U : 0U) + (a3d ? 1U : 0U);
    if (a3c || a3d) {
        s_oled_ok   = 1U;
        s_oled_addr = a3c ? 0x3C : 0x3D;
        SH1106_SetAddress(s_oled_addr);
        SH1106_Init();
    } else {
        s_oled_ok = 0U;
        if (force_init) {
            s_oled_addr = 0x3C;
            SH1106_SetAddress(s_oled_addr);
            SH1106_Init();
        }
    }
}

static void draw_boot_splash(void)
{
    char buf[24];
    SH1106_Clear();
    SH1106_DrawString(0,  4, "LF TED 2026 - SV UGM");
    SH1106_DrawLine(0, 14, 127, 14, true);
    SH1106_DrawString(0, 22, "SENS + CALIB + MOTOR");
    snprintf(buf, sizeof(buf), "Calib: %s", s_flash_loaded ? "FLASH OK" : "DEFAULT");
    SH1106_DrawString(0, 34, buf);
    snprintf(buf, sizeof(buf), "OLED: 0x%02X (%s)",
             (unsigned)s_oled_addr,
             (g_sh1106_swapped != 0U) ? "BitB" : "I2C1");
    SH1106_DrawString(0, 50, buf);
    SH1106_Update();
}

static void show_flash_message(const char *line1, const char *line2, uint32_t hold_ms)
{
    SH1106_Clear();
    SH1106_DrawRect(0, 0, SH1106_WIDTH, SH1106_HEIGHT, true);
    SH1106_DrawString(8, 20, line1);
    SH1106_DrawString(8, 34, line2);
    SH1106_Update();
    if (hold_ms > 0U) {
        HAL_Delay(hold_ms);
    }
}

static void motor_disarm_safe(void)
{
    s_motor_armed = 0U;
    Motor_Coast();
    s_cmd_left  = 0;
    s_cmd_right = 0;
}

static void motor_compute_manual_lr(int16_t *out_l, int16_t *out_r)
{
    switch (s_manual_target) {
    case 1U: /* KIRI SAJA */
        *out_l = s_manual_spd;
        *out_r = 0;
        break;
    case 2U: /* KANAN SAJA */
        *out_l = 0;
        *out_r = s_manual_spd;
        break;
    case 3U: /* PUTAR KIRI/KANAN */
        *out_l = (int16_t)(-s_manual_spd);
        *out_r = s_manual_spd;
        break;
    case 0U: /* KIRI + KANAN */
    default:
        *out_l = s_manual_spd;
        *out_r = s_manual_spd;
        break;
    }
}

static void apply_manual_profile(uint8_t idx)
{
    const ManualProfile_t *mp = &MANUAL_PROFILES[idx];
    s_manual_profile = idx;
    s_manual_target  = mp->target;
    Motor_SetFreqHz(mp->freq_hz);
    Motor_SetSlowDecay(mp->slow_decay);
}

static void motor_apply_current(void)
{
    if (!s_motor_armed) {
        Motor_Coast();
        s_cmd_left  = 0;
        s_cmd_right = 0;
        return;
    }

    if (s_mode == MODE_MOTOR_MANUAL) {
        int16_t l = 0;
        int16_t r = 0;
        motor_compute_manual_lr(&l, &r);
        Motor_Set(l, r);
        s_cmd_left  = l;
        s_cmd_right = r;
        return;
    }

    const MotorPhase_t *p = &MOTOR_PHASES[s_motor_phase];
    switch (p->kind) {
    case MP_BRAKE:
        Motor_Brake();
        s_cmd_left  = 0;
        s_cmd_right = 0;
        break;

    case MP_WALK:
        Motor_SetChannelRaw(p->ch, MOTOR_PWM_ARR);
        s_cmd_left  = (p->ch == 3U) ?  MOTOR_CMD_MAX : (p->ch == 2U) ? -MOTOR_CMD_MAX : 0;
        s_cmd_right = (p->ch == 1U) ?  MOTOR_CMD_MAX : (p->ch == 0U) ? -MOTOR_CMD_MAX : 0;
        break;

    case MP_SET:
    default:
        Motor_Set(p->l, p->r);
        s_cmd_left  = p->l;
        s_cmd_right = p->r;
        break;
    }
}

static void enter_mode(TestMode_t m)
{
    motor_disarm_safe();
    Motor_SetFreqHz(1000U);
    Motor_SetSlowDecay(false);
    s_mode       = m;
    s_oled_dirty = 1U;
    Buttons_Drain();

    if (m == MODE_SENSOR_TEST) {
        s_sens_page = 0U;
        s_sens_hold = 0U;
    } else if (m == MODE_CALIB) {
        s_calib_running = 0U;
        s_calib_page    = 0U;
    } else if (m == MODE_MOTOR_PRESET) {
        s_motor_phase = 0U;
    } else if (m == MODE_MOTOR_MANUAL) {
        s_manual_spd = 400;
        apply_manual_profile(0U);
    } else if (m == MODE_OLED_TEST) {
        s_oled_page        = 0U;
        s_oled_checker_inv = 0U;
        s_oled_frame_mode  = 0U;
    } else if (m == MODE_BUTTON_TEST) {
        snprintf(s_btn_last_evt, sizeof(s_btn_last_evt), "Tekan tombol...");
        s_back_last_ms  = 0U;
        s_back_hold_t0  = 0U;
        s_prev_raw_mask = Buttons_GetRawMask();
    }
}

/* ============================================================
 * Render tiap layar OLED
 * ============================================================ */
static void draw_pwm_bar(int x, int y, int w, int h, int16_t cmd)
{
    int mid = x + w / 2;
    if (cmd > MOTOR_CMD_MAX) {
        cmd = MOTOR_CMD_MAX;
    }
    if (cmd < -MOTOR_CMD_MAX) {
        cmd = -MOTOR_CMD_MAX;
    }

    SH1106_DrawRect(x, y, w, h, true);
    SH1106_DrawLine(mid, y, mid, y + h - 1, true);

    int len = (int)cmd * (w / 2) / MOTOR_CMD_MAX;
    if (len > 0) {
        SH1106_DrawFillRect(mid, y + 1, len, h - 2, true);
    } else if (len < 0) {
        SH1106_DrawFillRect(mid + len, y + 1, -len, h - 2, true);
    }
}

static void render_menu(void)
{
    static const char *const ITEMS[MENU_ITEM_COUNT] = {
        "1.Test Sensor 16CH",
        "2.Kalibrasi Sensor",
        "3.Test Motor Preset",
        "4.Test Motor Manual",
        "5.Test Layar OLED",
        "6.Test Tombol & Pin"
    };
    char buf[24];

    if (s_menu_idx < s_menu_top) {
        s_menu_top = s_menu_idx;
    } else if (s_menu_idx >= (uint8_t)(s_menu_top + MENU_VISIBLE_ROWS)) {
        s_menu_top = (uint8_t)(s_menu_idx - MENU_VISIBLE_ROWS + 1U);
    }

    SH1106_Clear();
    SH1106_DrawString(0, 0, "== TEST LF TED26 ==");

    for (uint8_t row = 0U; row < MENU_VISIBLE_ROWS; row++) {
        uint8_t idx = (uint8_t)(s_menu_top + row);
        if (idx >= MENU_ITEM_COUNT) {
            break;
        }
        snprintf(buf, sizeof(buf), "%c%s",
                 (idx == s_menu_idx) ? '>' : ' ', ITEMS[idx]);
        SH1106_DrawString(0, 12 + (int)row * 10, buf);
    }

    /* Indikator scroll kanan jika masih ada item di atas/bawah */
    if (s_menu_top > 0U) {
        SH1106_DrawChar(122, 12, '^');
    }
    if ((uint8_t)(s_menu_top + MENU_VISIBLE_ROWS) < MENU_ITEM_COUNT) {
        SH1106_DrawChar(122, 42, 'v');
    }

    SH1106_DrawLine(0, 53, 127, 53, true);
    SH1106_DrawString(0, 56, "U/D:Pilih  OK:Masuk");
    SH1106_Update();
}

static void render_sensor_test(void)
{
    char buf[24];
    SH1106_Clear();

    if (s_sens_page == 0U) {
        /* Halaman 1/4: Bar Normalized (0..1000) + Marker Posisi */
        snprintf(buf, sizeof(buf), "NORM 1/4 %s %s",
                 s_params.invert ? "INV" : "NRM",
                 s_sens_hold ? "[H]" : "   ");
        SH1106_DrawString(0, 0, buf);

        const int barW = 6, gap = 2, x0 = 1, base = 42;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            int h = (int)((uint32_t)s_disp_norm[i] * 26U / (uint32_t)SENSOR_NORM_MAX);
            int x = x0 + i * (barW + gap);
            if (h > 0) {
                SH1106_DrawFillRect(x, base - h, barW, h, true);
            } else {
                SH1106_DrawLine(x, base, x + barW - 1, base, true);
            }
        }

        /* Marker segitiga posisi (-7500..+7500 -> x = 6..122) */
        if (s_disp_line_vis) {
            int mx = 64 + ((int)s_disp_pos * 58) / SENSOR_POS_MAX;
            SH1106_DrawLine(mx - 2, 10, mx + 2, 10, true);
            SH1106_DrawLine(mx - 2, 10, mx,     13, true);
            SH1106_DrawLine(mx + 2, 10, mx,     13, true);
        }

        snprintf(buf, sizeof(buf), "pos:%+6d %s",
                 (int)s_disp_pos, s_disp_line_vis ? "LINE" : "LOST");
        SH1106_DrawString(0, 46, buf);

        snprintf(buf, sizeof(buf), "t:%4luus HldOK:Inv",
                 (unsigned long)s_disp_scan_us);
        SH1106_DrawString(0, 56, buf);
    } else if (s_sens_page == 1U) {
        /* Halaman 2/4: Bar Raw ADC (0..4095) */
        uint16_t mn = ADC_MAX;
        uint16_t mx = 0U;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (s_disp_raw[i] < mn) { mn = s_disp_raw[i]; }
            if (s_disp_raw[i] > mx) { mx = s_disp_raw[i]; }
        }

        snprintf(buf, sizeof(buf), "RAW BAR 2/4 %s", s_sens_hold ? "[HOLD]" : "      ");
        SH1106_DrawString(0, 0, buf);

        const int barW = 6, gap = 2, x0 = 1, base = 42;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            int h = (int)((uint32_t)s_disp_raw[i] * 28U / (uint32_t)ADC_MAX);
            int x = x0 + i * (barW + gap);
            if (h > 0) {
                SH1106_DrawFillRect(x, base - h, barW, h, true);
            } else {
                SH1106_DrawLine(x, base, x + barW - 1, base, true);
            }
        }

        snprintf(buf, sizeof(buf), "Mn:%4u Mx:%4u E:%lu",
                 (unsigned)mn, (unsigned)mx, (unsigned long)Sensor_GetAdcErr());
        SH1106_DrawString(0, 46, buf);
        SH1106_DrawString(0, 56, "U/D:Hal OK:Hold BK:Ex");
    } else {
        /* Halaman 3/4 (CH00..07) & 4/4 (CH08..15): Angka Raw */
        uint8_t base_ch = (s_sens_page == 2U) ? 0U : 8U;
        snprintf(buf, sizeof(buf), "RAW CH%02u-%02u %u/4 %s",
                 (unsigned)base_ch, (unsigned)(base_ch + 7U),
                 (unsigned)(s_sens_page + 1U),
                 s_sens_hold ? "H" : " ");
        SH1106_DrawString(0, 0, buf);

        for (uint8_t r = 0U; r < 4U; r++) {
            uint8_t c0 = (uint8_t)(base_ch + r * 2U);
            uint8_t c1 = (uint8_t)(c0 + 1U);
            snprintf(buf, sizeof(buf), "C%02u:%4u  C%02u:%4u",
                     (unsigned)c0, (unsigned)s_disp_raw[c0],
                     (unsigned)c1, (unsigned)s_disp_raw[c1]);
            SH1106_DrawString(0, 12 + (int)r * 10, buf);
        }
        SH1106_DrawString(0, 56, "U/D:Hal OK:Hold BK:Ex");
    }

    SH1106_Update();
}

static void render_calib(void)
{
    char buf[24];
    SH1106_Clear();

    if (s_calib_running) {
        uint16_t cmin[SENSOR_COUNT];
        uint16_t cmax[SENSOR_COUNT];
        uint32_t samples = 0U;
        Calib_GetLiveMinMax(cmin, cmax, &samples);

        uint8_t  ready = Calib_CountReadyChannels((uint16_t)SENSOR_CALIB_MIN_SPAN);
        uint16_t g_min = ADC_MAX;
        uint16_t g_max = 0U;

        SH1106_DrawString(0, 0, "KALIB! SAPU HTM+PTH");

        /* Bar spread (max - min) tiap channel 0..15 */
        const int barW = 6, gap = 2, x0 = 1, base = 36;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            uint16_t span = (cmax[i] > cmin[i]) ? (uint16_t)(cmax[i] - cmin[i]) : 0U;
            if (cmin[i] < g_min) { g_min = cmin[i]; }
            if (cmax[i] > g_max) { g_max = cmax[i]; }
            int h = (int)((uint32_t)span * 24U / (uint32_t)ADC_MAX);
            int x = x0 + i * (barW + gap);
            if (h > 0) {
                SH1106_DrawFillRect(x, base - h, barW, h, true);
            } else {
                SH1106_DrawLine(x, base, x + barW - 1, base, true);
            }
        }

        snprintf(buf, sizeof(buf), "Siap:%2u/16 Smp:%4lu",
                 (unsigned)ready, (unsigned long)(samples % 10000U));
        SH1106_DrawString(0, 39, buf);

        snprintf(buf, sizeof(buf), "Mn:%4u  Mx:%4u",
                 (unsigned)((samples > 0U) ? g_min : 0U),
                 (unsigned)g_max);
        SH1106_DrawString(0, 48, buf);

        SH1106_DrawString(0, 56, "OK:Simpan  BK:Batal");
    } else if (s_calib_page == 0U) {
        uint32_t sum_span = 0U;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (s_params.cal_max[i] > s_params.cal_min[i]) {
                sum_span += (uint32_t)(s_params.cal_max[i] - s_params.cal_min[i]);
            }
        }
        uint32_t avg_span = sum_span / SENSOR_COUNT;

        SH1106_DrawString(0, 0, "2. KALIBRASI 1/5");
        snprintf(buf, sizeof(buf), "Flash: %s",
                 s_flash_loaded ? "TERSIMPAN" : "DEFAULT");
        SH1106_DrawString(0, 11, buf);

        snprintf(buf, sizeof(buf), "Garis: %s",
                 s_params.invert ? "INVERT (PTH)" : "NORMAL (HTM)");
        SH1106_DrawString(0, 21, buf);

        snprintf(buf, sizeof(buf), "Avg Span: %4lu", (unsigned long)avg_span);
        SH1106_DrawString(0, 31, buf);

        SH1106_DrawString(0, 44, "OK:Mulai  HldOK:Inv");
        SH1106_DrawString(0, 55, "U/D:Data  BK:Menu");
    } else {
        /* Halaman 2..5: Preview cal_min & cal_max tersimpan */
        bool    is_max  = (s_calib_page == 2U || s_calib_page == 4U);
        uint8_t base_ch = (s_calib_page >= 3U) ? 8U : 0U;
        const uint16_t *arr = is_max ? s_params.cal_max : s_params.cal_min;

        snprintf(buf, sizeof(buf), "%s CH%02u-%02u %u/5",
                 is_max ? "MAX" : "MIN",
                 (unsigned)base_ch, (unsigned)(base_ch + 7U),
                 (unsigned)(s_calib_page + 1U));
        SH1106_DrawString(0, 0, buf);

        for (uint8_t r = 0U; r < 4U; r++) {
            uint8_t c0 = (uint8_t)(base_ch + r * 2U);
            uint8_t c1 = (uint8_t)(c0 + 1U);
            snprintf(buf, sizeof(buf), "C%02u:%4u  C%02u:%4u",
                     (unsigned)c0, (unsigned)arr[c0],
                     (unsigned)c1, (unsigned)arr[c1]);
            SH1106_DrawString(0, 12 + (int)r * 10, buf);
        }
        SH1106_DrawString(0, 56, "OK:Kalib U/D:Hal BK:Ex");
    }

    SH1106_Update();
}

static void render_motor_preset(void)
{
    char buf[24];
    const MotorPhase_t *p = &MOTOR_PHASES[s_motor_phase];

    SH1106_Clear();
    snprintf(buf, sizeof(buf), "MOTOR %02u/%02u %s",
             (unsigned)(s_motor_phase + 1U),
             (unsigned)MOTOR_PHASE_COUNT,
             s_motor_armed ? "[RUN!]" : "[STOP]");
    SH1106_DrawString(0, 0, buf);

    SH1106_DrawString(0, 12, p->name);

    snprintf(buf, sizeof(buf), "L:%+5d   R:%+5d", (int)s_cmd_left, (int)s_cmd_right);
    SH1106_DrawString(0, 24, buf);

    draw_pwm_bar(2,  36, 60, 12, s_cmd_left);
    draw_pwm_bar(66, 36, 60, 12, s_cmd_right);

    SH1106_DrawString(0, 55, "U/D:Fase OK:Run BK:Ex");
    SH1106_Update();
}

static void render_motor_manual(void)
{
    char buf[24];
    int16_t preview_l = 0;
    int16_t preview_r = 0;
    motor_compute_manual_lr(&preview_l, &preview_r);

    SH1106_Clear();
    snprintf(buf, sizeof(buf), "MANUAL %+5d %s",
             (int)s_manual_spd,
             s_motor_armed ? "[RUN!]" : "[STOP]");
    SH1106_DrawString(0, 0, buf);

    snprintf(buf, sizeof(buf), "Tgt: %s", MANUAL_PROFILES[s_manual_profile].label);
    SH1106_DrawString(0, 11, buf);

    snprintf(buf, sizeof(buf), "PWM:%4luHz %s",
             (unsigned long)Motor_GetFreqHz(),
             Motor_GetSlowDecay() ? "SLOW" : "FAST");
    SH1106_DrawString(0, 21, buf);

    snprintf(buf, sizeof(buf), "L:%+5d   R:%+5d",
             (int)(s_motor_armed ? s_cmd_left  : preview_l),
             (int)(s_motor_armed ? s_cmd_right : preview_r));
    SH1106_DrawString(0, 31, buf);

    draw_pwm_bar(2,  41, 60, 11, s_motor_armed ? s_cmd_left  : 0);
    draw_pwm_bar(66, 41, 60, 11, s_motor_armed ? s_cmd_right : 0);

    SH1106_DrawString(0, 55, "U/D:Spd OK:Run/HldOpt");
    SH1106_Update();
}

static void render_oled_test(void)
{
    char buf[24];
    SH1106_Clear();

    if (s_oled_page == 0U) {
        SH1106_DrawString(0, 0, "5. OLED TEST 1/5");
        snprintf(buf, sizeof(buf), "Addr:0x%02X Bus:%s",
                 (unsigned)s_oled_addr,
                 (g_sh1106_swapped != 0U) ? "BitBang" : "HW I2C1");
        SH1106_DrawString(0, 10, buf);
        snprintf(buf, sizeof(buf), "SYSCLK: %lu MHz",
                 (unsigned long)(SystemCoreClock / 1000000UL));
        SH1106_DrawString(0, 20, buf);
        snprintf(buf, sizeof(buf), "I2C OK:%lu Err:%lu",
                 (unsigned long)g_sh1106_ok, (unsigned long)g_sh1106_fail);
        SH1106_DrawString(0, 29, buf);
        snprintf(buf, sizeof(buf), "Rec:%lu Up:%lus",
                 (unsigned long)g_sh1106_recover,
                 (unsigned long)(HAL_GetTick() / 1000U));
        SH1106_DrawString(0, 38, buf);
        SH1106_DrawString(0, 48, "OK:RstCnt U/D:Hal");
        SH1106_DrawString(0, 56, "BACK: Menu Utama");
    } else if (s_oled_page == 1U) {
        for (int y = 0; y < SH1106_HEIGHT; y += 8) {
            for (int x = 0; x < SH1106_WIDTH; x += 8) {
                uint8_t odd = (uint8_t)(((x / 8) + (y / 8)) & 1);
                if (odd ^ s_oled_checker_inv) {
                    SH1106_DrawFillRect(x, y, 8, 8, true);
                }
            }
        }
        SH1106_DrawFillRect(8, 22, 112, 20, false);
        SH1106_DrawRect(8, 22, 112, 20, true);
        SH1106_DrawString(12, 24, "CHECKER 2/5");
        SH1106_DrawString(12, 33, "OK:Inv U/D:Hal");
    } else if (s_oled_page == 2U) {
        if (!s_oled_frame_mode) {
            SH1106_DrawFillRect(0, 0, SH1106_WIDTH, SH1106_HEIGHT, true);
        } else {
            SH1106_DrawRect(0, 0, SH1106_WIDTH, SH1106_HEIGHT, true);
            SH1106_DrawRect(2, 2, SH1106_WIDTH - 4, SH1106_HEIGHT - 4, true);
            SH1106_DrawString(14, 18, "BINGKAI 128x64 3/5");
            SH1106_DrawString(14, 30, "OK:FullWhite");
            SH1106_DrawString(14, 42, "U/D:Hal  BK:Menu");
        }
    } else if (s_oled_page == 3U) {
        SH1106_DrawString(0, 0, "GEO & FONT 4/5");
        SH1106_DrawCircle(14, 24, 10, true);
        SH1106_DrawFillCircle(40, 24, 9, true);
        SH1106_DrawRect(56, 14, 22, 20, true);
        SH1106_DrawFillRect(84, 14, 20, 20, true);
        SH1106_DrawLine(110, 14, 125, 33, true);
        SH1106_DrawLine(125, 14, 110, 33, true);
        SH1106_DrawString(0, 38, "ABCabc 012345 !@#?");
        SH1106_DrawString(0, 48, "SH1106 128x64 OK");
        SH1106_DrawString(0, 56, "U/D:Hal  BK:Menu");
    } else {
        const int barW = 5, gap = 2, x0 = 9, base = 44;
        s_anim_x10 += s_anim_dir * 6;
        if (s_anim_x10 >= 145) { s_anim_x10 = 145; s_anim_dir = -1; }
        if (s_anim_x10 <= 5)   { s_anim_x10 = 5;   s_anim_dir =  1; }

        SH1106_DrawString(0, 0, "ANIMASI BAR 5/5");
        for (int i = 0; i < 16; i++) {
            int d = i * 10 - s_anim_x10;
            if (d < 0) { d = -d; }
            int h = 30 - d;
            if (h < 2) { h = 2; }
            if (h > 30) { h = 30; }
            int x = x0 + i * (barW + gap);
            SH1106_DrawFillRect(x, base - h, barW, h, true);
        }
        uint32_t fps = (s_last_frame_ms > 0U) ? (1000U / s_last_frame_ms) : 0U;
        snprintf(buf, sizeof(buf), "Frm:%lums ~%luFPS",
                 (unsigned long)s_last_frame_ms, (unsigned long)fps);
        SH1106_DrawString(0, 47, buf);
        SH1106_DrawString(0, 56, "U/D:Hal  BK:Menu");
    }

    SH1106_Update();
}

static void render_button_test(void)
{
    static const char *const LABELS[4] = { "UP", "DN", "OK", "BK" };
    uint8_t mask = Buttons_GetRawMask();
    char buf[24];

    SH1106_Clear();
    SH1106_DrawString(0, 0, "6. TEST TOMBOL & PIN");

    for (int i = 0; i < 4; i++) {
        int x = 4 + i * 31;
        if (mask & (uint8_t)(1U << i)) {
            SH1106_DrawFillRect(x, 12, 26, 14, true);
        } else {
            SH1106_DrawRect(x, 12, 26, 14, true);
        }
        SH1106_DrawString(x + 7, 28, LABELS[i]);
    }

    snprintf(buf, sizeof(buf), "U:%2lu D:%2lu O:%2lu B:%2lu",
             (unsigned long)(s_btn_cnt[0] % 100U),
             (unsigned long)(s_btn_cnt[1] % 100U),
             (unsigned long)(s_btn_cnt[2] % 100U),
             (unsigned long)(s_btn_cnt[3] % 100U));
    SH1106_DrawString(0, 38, buf);
    SH1106_DrawString(0, 47, s_btn_last_evt);
    SH1106_DrawString(0, 56, "2x/Hold BACK: Menu");
    SH1106_Update();
}

/* ============================================================
 * Proses input per mode
 * ============================================================ */
static void handle_input(void)
{
    Button_t evt_btn = BTN_NONE;
    uint8_t  is_long = 0U;

    while (Buttons_PopEvent(&evt_btn, &is_long)) {
        Button_t press  = (!is_long) ? evt_btn : BTN_NONE;
        Button_t lpress = (is_long)  ? evt_btn : BTN_NONE;
        s_oled_dirty = 1U;

        switch (s_mode) {
        case MODE_MENU:
            if (press == BTN_UP) {
                s_menu_idx = (s_menu_idx == 0U)
                           ? (MENU_ITEM_COUNT - 1U)
                           : (uint8_t)(s_menu_idx - 1U);
            } else if (press == BTN_DOWN) {
                s_menu_idx = (uint8_t)((s_menu_idx + 1U) % MENU_ITEM_COUNT);
            } else if (press == BTN_OK) {
                enter_mode((TestMode_t)(MODE_SENSOR_TEST + s_menu_idx));
                return;
            }
            break;

        case MODE_SENSOR_TEST:
            if (press == BTN_UP) {
                s_sens_page = (s_sens_page == 0U)
                            ? (SENSOR_TEST_PAGES - 1U)
                            : (uint8_t)(s_sens_page - 1U);
            } else if (press == BTN_DOWN) {
                s_sens_page = (uint8_t)((s_sens_page + 1U) % SENSOR_TEST_PAGES);
            } else if (press == BTN_OK) {
                s_sens_hold = (uint8_t)!s_sens_hold;
            } else if (lpress == BTN_OK) {
                s_params.invert = (uint8_t)!s_params.invert;
            } else if (press == BTN_BACK || lpress == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_CALIB:
            if (s_calib_running) {
                if (press == BTN_OK || lpress == BTN_OK) {
                    s_calib_running = 0U;
                    Calib_Finish(&s_params);
                    show_flash_message("MENYIMPAN FLASH...", "Sektor 5 (128KB)", 0U);
                    bool saved = Storage_Save(&s_params);
                    if (saved) {
                        s_flash_loaded = 1U;
                        show_flash_message("KALIBRASI SUKSES!", "Tersimpan di Flash", 900U);
                    } else {
                        show_flash_message("GAGAL SIMPAN!", "Aktif di RAM saja", 900U);
                    }
                    enter_mode(MODE_SENSOR_TEST);
                    return;
                } else if (press == BTN_BACK || lpress == BTN_BACK) {
                    s_calib_running = 0U;
                    Buttons_Drain();
                }
            } else {
                if (press == BTN_UP) {
                    s_calib_page = (s_calib_page == 0U)
                                 ? (CALIB_VIEW_PAGES - 1U)
                                 : (uint8_t)(s_calib_page - 1U);
                } else if (press == BTN_DOWN) {
                    s_calib_page = (uint8_t)((s_calib_page + 1U) % CALIB_VIEW_PAGES);
                } else if (press == BTN_OK) {
                    Calib_Start();
                    s_calib_running = 1U;
                } else if (lpress == BTN_OK) {
                    s_params.invert = (uint8_t)!s_params.invert;
                    show_flash_message("SIMPAN MODE GARIS",
                                       s_params.invert ? "INVERT (Putih)" : "NORMAL (Hitam)",
                                       0U);
                    if (Storage_Save(&s_params)) {
                        s_flash_loaded = 1U;
                    }
                    HAL_Delay(500U);
                    Buttons_Drain();
                } else if (press == BTN_BACK || lpress == BTN_BACK) {
                    enter_mode(MODE_MENU);
                    return;
                }
            }
            break;

        case MODE_MOTOR_PRESET:
            if (press == BTN_UP) {
                motor_disarm_safe();
                s_motor_phase = (s_motor_phase == 0U)
                              ? (MOTOR_PHASE_COUNT - 1U)
                              : (uint8_t)(s_motor_phase - 1U);
            } else if (press == BTN_DOWN) {
                motor_disarm_safe();
                s_motor_phase = (uint8_t)((s_motor_phase + 1U) % MOTOR_PHASE_COUNT);
            } else if (press == BTN_OK) {
                s_motor_armed = (uint8_t)!s_motor_armed;
                motor_apply_current();
            } else if (press == BTN_BACK || lpress == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_MOTOR_MANUAL:
            if (press == BTN_UP || lpress == BTN_UP) {
                if (s_manual_spd <= (MOTOR_CMD_MAX - MANUAL_SPD_STEP)) {
                    s_manual_spd = (int16_t)(s_manual_spd + MANUAL_SPD_STEP);
                } else {
                    s_manual_spd = MOTOR_CMD_MAX;
                }
                if (s_motor_armed) {
                    motor_apply_current();
                }
            } else if (press == BTN_DOWN || lpress == BTN_DOWN) {
                if (s_manual_spd >= (-MOTOR_CMD_MAX + MANUAL_SPD_STEP)) {
                    s_manual_spd = (int16_t)(s_manual_spd - MANUAL_SPD_STEP);
                } else {
                    s_manual_spd = -MOTOR_CMD_MAX;
                }
                if (s_motor_armed) {
                    motor_apply_current();
                }
            } else if (press == BTN_OK) {
                s_motor_armed = (uint8_t)!s_motor_armed;
                motor_apply_current();
            } else if (lpress == BTN_OK) {
                motor_disarm_safe();
                apply_manual_profile((uint8_t)((s_manual_profile + 1U) % MANUAL_PROFILE_COUNT));
            } else if (press == BTN_BACK || lpress == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_OLED_TEST:
            if (press == BTN_UP) {
                s_oled_page = (s_oled_page == 0U)
                            ? (OLED_TEST_PAGES - 1U)
                            : (uint8_t)(s_oled_page - 1U);
            } else if (press == BTN_DOWN) {
                s_oled_page = (uint8_t)((s_oled_page + 1U) % OLED_TEST_PAGES);
            } else if (press == BTN_OK) {
                if (s_oled_page == 0U) {
                    g_sh1106_ok      = 0U;
                    g_sh1106_fail    = 0U;
                    g_sh1106_recover = 0U;
                } else if (s_oled_page == 1U) {
                    s_oled_checker_inv = (uint8_t)!s_oled_checker_inv;
                } else if (s_oled_page == 2U) {
                    s_oled_frame_mode = (uint8_t)!s_oled_frame_mode;
                }
            } else if (press == BTN_BACK || lpress == BTN_BACK) {
                enter_mode(MODE_MENU);
                return;
            }
            break;

        case MODE_BUTTON_TEST: {
            static const char *const BNAMES[4] = { "UP", "DOWN", "OK", "BACK" };
            uint32_t now = HAL_GetTick();
            if (press != BTN_NONE) {
                int idx = (int)press - (int)BTN_UP;
                if (idx >= 0 && idx < 4) {
                    s_btn_cnt[idx]++;
                    snprintf(s_btn_last_evt, sizeof(s_btn_last_evt),
                             "Last: %s SHORT", BNAMES[idx]);
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
                }
                int idx = (int)lpress - (int)BTN_UP;
                if (idx >= 0 && idx < 4) {
                    snprintf(s_btn_last_evt, sizeof(s_btn_last_evt),
                             "Last: %s LONG", BNAMES[idx]);
                }
            }
            break;
        }
        }
    }

    /* Hentikan motor seketika saat tombol BACK ditekan di mode motor */
    if ((s_mode == MODE_MOTOR_PRESET || s_mode == MODE_MOTOR_MANUAL) && s_motor_armed) {
        if (Buttons_GetRawMask() & (1U << 3)) {
            motor_disarm_safe();
            s_oled_dirty = 1U;
        }
    }

    /* Update instan kotak tombol & deteksi tahan BACK >= 400 ms di MODE_BUTTON_TEST */
    if (s_mode == MODE_BUTTON_TEST) {
        uint8_t raw_mask = Buttons_GetRawMask();
        if (raw_mask != s_prev_raw_mask) {
            s_prev_raw_mask = raw_mask;
            s_oled_dirty    = 1U;
        }
        uint32_t now = HAL_GetTick();
        if (raw_mask & (1U << 3)) {
            if (s_back_hold_t0 == 0U) {
                s_back_hold_t0 = (now != 0U) ? now : 1U;
            } else if ((now - s_back_hold_t0) >= 400U) {
                enter_mode(MODE_MENU);
            }
        } else {
            s_back_hold_t0 = 0U;
        }
    }
}

/* ============================================================
 * Siklus hidup aplikasi utama
 * ============================================================ */
void App_Init(void)
{
    /* LED PC13 mati awal (active LOW) & motor dipastikan COAST secepat mungkin */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    Motor_Init();
    Motor_Coast();

    Sensor_Init();
    Buttons_Init();

    if (Storage_Load(&s_params)) {
        s_flash_loaded = 1U;
    } else {
        s_flash_loaded = 0U;
        Params_SetDefaults(&s_params);
    }

    /* Jeda stabilisasi catu daya sebelum diagnosa pin & komunikasi I2C OLED */
    HAL_Delay(BOOT_SETTLE_MS);
    SH1106_DiagPins();
    oled_init_safe(true);

    draw_boot_splash();
    HAL_Delay(BOOT_SPLASH_MS);

    enter_mode(MODE_MENU);
    s_last_oled       = 0U;
    s_last_oled_retry = HAL_GetTick();
    s_last_led        = HAL_GetTick();
}

void App_Update(void)
{
    uint32_t now = HAL_GetTick();

    handle_input();

    /* Pembacaan sensor 16CH saat di MODE_SENSOR_TEST atau kalibrasi aktif */
    if (s_mode == MODE_SENSOR_TEST) {
        uint16_t raw[SENSOR_COUNT];
        uint16_t norm[SENSOR_COUNT];
        bool     vis = false;
        Sensor_ReadRaw(raw);
        Calib_Normalize(&s_params, raw, norm);
        int16_t pos = Sensor_ComputePosition(norm, &vis);
        if (!s_sens_hold) {
            for (int i = 0; i < SENSOR_COUNT; i++) {
                s_disp_raw[i]  = raw[i];
                s_disp_norm[i] = norm[i];
            }
            s_disp_pos      = pos;
            s_disp_line_vis = vis;
            s_disp_scan_us  = Sensor_GetLastScanUs();
        }
    } else if (s_mode == MODE_CALIB && s_calib_running) {
        uint16_t raw[SENSOR_COUNT];
        Sensor_ReadRaw(raw);
        Calib_Update(raw);
    }

    /* Heartbeat PC13: cepat (100 ms) saat motor [RUN!] atau kalibrasi aktif */
    uint32_t led_period = (s_motor_armed || (s_mode == MODE_CALIB && s_calib_running)) ? 100U : 500U;
    if ((now - s_last_led) >= led_period) {
        s_last_led = now;
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    }

    /* Hot-plug retry jika OLED belum terdeteksi saat boot */
    if (!s_oled_ok && (now - s_last_oled_retry) >= 500U) {
        s_last_oled_retry = now;
        oled_init_safe(false);
        if (s_oled_ok) {
            s_oled_dirty = 1U;
        }
    }

    /* Dirty-driven OLED refresh */
    bool live_mode = (s_mode == MODE_SENSOR_TEST)
                  || (s_mode == MODE_CALIB && s_calib_running)
                  || (s_mode == MODE_OLED_TEST && s_oled_page == 4U);
    uint32_t bg_period = live_mode ? OLED_LIVE_MS : OLED_BG_REFRESH_MS;

    if ((s_oled_ok && (now - s_last_oled) >= bg_period) || s_oled_dirty) {
        uint32_t t0 = HAL_GetTick();
        s_oled_dirty = 0U;
        switch (s_mode) {
        case MODE_MENU:         render_menu();         break;
        case MODE_SENSOR_TEST:  render_sensor_test();  break;
        case MODE_CALIB:        render_calib();        break;
        case MODE_MOTOR_PRESET: render_motor_preset(); break;
        case MODE_MOTOR_MANUAL: render_motor_manual(); break;
        case MODE_OLED_TEST:    render_oled_test();    break;
        case MODE_BUTTON_TEST:  render_button_test();  break;
        }
        uint32_t t1 = HAL_GetTick();
        s_last_frame_ms = t1 - t0;
        s_last_oled     = t1;
    }

    HAL_Delay(1);
}
