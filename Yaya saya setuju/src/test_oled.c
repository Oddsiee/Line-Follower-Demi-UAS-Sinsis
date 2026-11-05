#include "test_oled.h"
#include "sh1106.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

#define SCREEN_COUNT 4U
#define SCREEN_MS    2000U   /* lama tiap layar demo (ms) */
#define ANIM_MS      20U     /* laju animasi "garis"       */

static uint32_t s_last      = 0;
static uint32_t s_anim_last = 0;
static uint8_t  s_screen    = 0;

/* Posisi "garis" dalam 1/10 indeks sensor (5..145 -> 0.5..14.5). */
static int16_t s_center_x10 = 75;
static int16_t s_dir        = 1;

static bool s_oled_3c = false;
static bool s_oled_3d = false;

static void scan_i2c(void)
{
    s_oled_3c = SH1106_Probe(0x3C);
    s_oled_3d = SH1106_Probe(0x3D);
}

static void anim_tick(void)
{
    s_center_x10 += s_dir * 4;
    if (s_center_x10 >= 145) { s_center_x10 = 145; s_dir = -1; }
    if (s_center_x10 <= 5)   { s_center_x10 = 5;   s_dir =  1; }
}

/* ---- Layar demo ---- */
static void screen_boot(void)
{
    SH1106_DrawString(0, 8,  "SH1106 OLED TEST");
    SH1106_DrawString(0, 24, "Line Follower");
    SH1106_DrawString(0, 40, "TED 2026 - SV UGM");
    SH1106_DrawString(0, 56, "PB6/PB7 I2C1");
}

static void screen_scan(void)
{
    SH1106_DrawString(0, 8, "I2C SCAN");
    if (s_oled_3c) {
        SH1106_DrawString(0, 24, "OLED @ 0x3C  OK");
    } else if (s_oled_3d) {
        SH1106_DrawString(0, 24, "OLED @ 0x3D  OK");
    } else {
        SH1106_DrawString(0, 24, "OLED TIDAK TERDETEKSI");
    }
    SH1106_DrawString(0, 40, "cek SCL/SDA/VCC/GND");
    SH1106_DrawString(0, 48, "dan alamat I2C");
}

static void screen_shapes(void)
{
    SH1106_DrawString(0, 0, "SHAPES");
    SH1106_DrawCircle(20, 36, 15, true);
    SH1106_DrawFillCircle(52, 36, 13, true);
    SH1106_DrawRect(70, 22, 24, 26, true);
    SH1106_DrawFillRect(102, 22, 20, 26, true);
    SH1106_DrawLine(4, 60, 124, 60, true);
    SH1106_DrawLine(4, 60, 4, 8, true);
}

static void screen_live(void)
{
    const int barW = 5, gap = 2, x0 = 4, base = 46;
    int16_t pos = (int16_t)((s_center_x10 - 75) * 100);

    SH1106_DrawString(0, 0, "LIVE (simulasi)");

    for (int i = 0; i < 16; i++) {
        int d = i * 10 - s_center_x10;
        if (d < 0) d = -d;
        int v = 34 - d * 3;
        if (v < 0) v = 0;
        if (v > 34) v = 34;
        int x = x0 + i * (barW + gap);
        if (v > 0) SH1106_DrawFillRect(x, base - v, barW, v, true);
    }

    int mx = x0 + (s_center_x10 / 10) * (barW + gap) - 2;   /* marker */
    SH1106_DrawLine(mx - 3, base - 36, mx + 3, base - 36, true);
    SH1106_DrawLine(mx - 3, base - 36, mx,     base - 30, true);
    SH1106_DrawLine(mx + 3, base - 36, mx,     base - 30, true);

    char buf[20];
    snprintf(buf, sizeof(buf), "pos: %+5d", pos);
    SH1106_DrawString(0, 56, buf);
}

void TestOled_Init(void)
{
    scan_i2c();                 /* deteksi alamat sebelum init */
    if (s_oled_3c) {
        SH1106_SetAddress(0x3C);
    } else if (s_oled_3d) {
        SH1106_SetAddress(0x3D);
    }
    SH1106_Init();
    s_last      = HAL_GetTick();
    s_anim_last = s_last;
    s_screen    = 0;
    s_center_x10 = 75;
    s_dir        = 1;
}

void TestOled_Run(void)
{
    uint32_t now = HAL_GetTick();

    if (now - s_last >= SCREEN_MS) {
        s_last = now;
        s_screen = (uint8_t)((s_screen + 1) % SCREEN_COUNT);
    }
    if (now - s_anim_last >= ANIM_MS) {
        s_anim_last = now;
        anim_tick();
    }

    SH1106_Clear();
    switch (s_screen) {
        case 0: screen_boot();   break;
        case 1: screen_scan();   break;
        case 2: screen_shapes(); break;
        case 3: screen_live();   break;
    }
    SH1106_Update();
}
