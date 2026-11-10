#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Driver OLED 1.3" SH1106 (128x64) — pakai HAL I2C.
 * Bisa dipakai ulang untuk modul ui.c nanti.
 * ============================================================ */

#define SH1106_WIDTH   128
#define SH1106_HEIGHT  64

/* Alamat I2C 7-bit (default 0x3C; sebagian modul 0x3D). */
#ifndef SH1106_I2C_ADDR
#define SH1106_I2C_ADDR 0x3C
#endif

/* Handle I2C dari CubeMX (default hi2c1). Kalau pindah I2C2, ubah di sini. */
#ifndef SH1106_HI2C
#define SH1106_HI2C hi2c1
#endif

void SH1106_Init(void);          /* init + clear + tampilkan */
void SH1106_Clear(void);         /* bersihkan framebuffer    */
void SH1106_Update(void);        /* kirim framebuffer ke OLED */

/* true jika ada device yang ACK di alamat 7-bit (buat scan I2C) */
bool SH1106_Probe(uint8_t addr7);

/* Scan seluruh bus 0x08..0x77. Mengembalikan jumlah device yang ACK.
 * Bitmap: bit i = alamat (0x08+i) untuk lo, (0x40+i) untuk hi. */
uint32_t SH1106_Scan(uint32_t *lo, uint32_t *hi);

/* Statistik diagnosa (bisa dibaca via SWD). */
extern volatile uint32_t g_sh1106_ok;
extern volatile uint32_t g_sh1106_fail;
extern volatile uint32_t g_sh1106_last;
extern volatile uint32_t g_sh1106_recover;
extern volatile uint32_t g_sh1106_scan_count;
extern volatile uint32_t g_sh1106_scan_lo;
extern volatile uint32_t g_sh1106_scan_hi;

/* Pilih alamat OLED hasil scan sebelum memanggil SH1106_Init(). */
void SH1106_SetAddress(uint8_t addr7);

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
