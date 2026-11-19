#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Driver OLED 1.3" SH1106 (128x64) — pakai HAL I2C1 (100 kHz).
 * Dilengkapi SWRST bus recovery dan auto-fallback bit-bang
 * yang terskala terhadap SystemCoreClock (16 MHz / 84 MHz).
 * ============================================================ */

#define SH1106_WIDTH   128
#define SH1106_HEIGHT  64

#ifndef SH1106_I2C_ADDR
#define SH1106_I2C_ADDR 0x3C
#endif

#ifndef SH1106_HI2C
#define SH1106_HI2C hi2c1
#endif

void SH1106_Init(void);
void SH1106_Clear(void);
void SH1106_Update(void);

bool SH1106_Probe(uint8_t addr7);
uint32_t SH1106_Scan(uint32_t *lo, uint32_t *hi);
void SH1106_SetAddress(uint8_t addr7);
void SH1106_DiagPins(void);

/* Statistik & info diagnostik bus I2C */
extern volatile uint32_t g_sh1106_ok;
extern volatile uint32_t g_sh1106_fail;
extern volatile uint32_t g_sh1106_last;
extern volatile uint32_t g_sh1106_recover;
extern volatile uint32_t g_sh1106_swapped;
extern volatile uint16_t g_sh1106_scl_pin;
extern volatile uint16_t g_sh1106_sda_pin;
extern volatile uint32_t g_sh1106_scan_count;
extern volatile uint32_t g_sh1106_scan_lo;
extern volatile uint32_t g_sh1106_scan_hi;
extern volatile uint32_t g_i2c_pd;
extern volatile uint32_t g_i2c_pu;
extern volatile uint32_t g_i2c_fl;
extern volatile uint8_t  g_probe_hw_3c;
extern volatile uint8_t  g_probe_bb_67;
extern volatile uint8_t  g_probe_bb_76;
extern volatile uint8_t  g_bb67_step;
extern volatile uint8_t  g_bb76_step;
extern volatile uint8_t  g_bb67_found;
extern volatile uint8_t  g_bb76_found;

/* ---- Drawing primitif ---- */
void SH1106_SetPixel(int x, int y, bool on);
void SH1106_DrawLine(int x0, int y0, int x1, int y1, bool on);
void SH1106_DrawRect(int x, int y, int w, int h, bool on);
void SH1106_DrawFillRect(int x, int y, int w, int h, bool on);
void SH1106_DrawCircle(int cx, int cy, int r, bool on);
void SH1106_DrawFillCircle(int cx, int cy, int r, bool on);

/* ---- Teks (font 6x8, ASCII 32..126, advance 6 px/karakter) ---- */
void SH1106_DrawChar(int x, int y, char c);
void SH1106_DrawString(int x, int y, const char *s);

#ifdef __cplusplus
}
#endif

#endif /* SH1106_H */
