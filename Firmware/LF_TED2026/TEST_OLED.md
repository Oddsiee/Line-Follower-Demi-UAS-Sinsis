# Test OLED SH1106 (STM32CubeIDE + HAL)

File yang terlibat:

| File | Fungsi |
|---|---|
| `Core/Inc/sh1106.h` / `Core/Src/sh1106.c` | Driver SH1106 mandiri (init, framebuffer, drawing, font 6x8). Tidak butuh library eksternal. |
| `Core/Inc/test_oled.h` / `Core/Src/test_oled.c` | Rutin test: scan I2C + 4 layar demo otomatis. |
| `Tools/gen_font.py` | (opsional) generator font 6x8, kalau mau ganti font. |

## 1. Konfigurasi `.ioc` (CubeMX)

Di project `LF_TED2026` (MCU STM32F411CEUx):

1. **Connectivity → I2C1 → pilih "I2C"** (bukan disable).
2. Pastikan pin otomatis terpasang: **PB6 = I2C1_SCL**, **PB7 = I2C1_SDA**.
3. **Configuration → I2C1 → Parameter Settings**:
   - I2C Speed Mode = **Fast Mode** (400 kHz). (100 kHz juga jalan, cuma refresh lebih lambat.)
   - Sisanya biarkan default.
4. **Project Manager** (pastikan): "Generate peripheral initialization as a pair of .c/.h files" **tidak** perlu dicentang — `hi2c1` otomatis jadi global di `main.c` (yang dipakai driver).
5. **Generate Code**.

> Kalau ada error compile `I2C_HandleTypeDef hi2c1` tidak dikenal, pastikan `MX_I2C1_Init()` dipanggil di `main()` dan `hi2c1` dideklarasikan global.

## 2. Masukkan file ke project

Salin ke folder project CubeIDE (lalu project akan otomatis mendeteksi):

- `Core/Inc/sh1106.h`, `Core/Inc/test_oled.h`
- `Core/Src/sh1106.c`, `Core/Src/test_oled.c`

(File-file ini sudah ada di repo; tinggal `git pull` / salin manual.)

## 3. Hubungkan di `main.c`

```c
/* USER CODE BEGIN Includes */
#include "test_oled.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
TestOled_Init();
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
  /* USER CODE BEGIN 3 */
  TestOled_Run();
  /* USER CODE END 3 */
}
/* USER CODE END WHILE */
```

## 4. Build & flash

- Build (Project → Build).
- Flash via **ST-Link** (Run → Debug, atau langsung pakai **STM32CubeProgrammer** dengan ST-LINK).

## Hasil yang diharapkan

Layar berganti otomatis tiap 2 detik:

1. **Boot** — judul + info chip.
2. **I2C scan** — `OLED @ 0x3C OK` (atau `0x3D`, atau `TIDAK TERDETEKSI`).
3. **Shapes** — lingkaran, kotak, garis.
4. **Live simulasi** — 16 batang "sensor" + marker garis yang jalan kiri-kanan.

## Troubleshooting

| Gejala | Solusi |
|---|---|
| Layar gelap total | Cek hasil scan (layar 2). Kalau `TIDAK TERDETEKSI`: cek kabel SCL/SDA/VCC/GND dan pastikan `MX_I2C1_Init()` jalan. |
| Terdeteksi di `0x3D` | Ubah `SH1106_I2C_ADDR` jadi `0x3D` di `sh1106.h`. |
| Gambar terbalik / mirror | Tukar `0xA1`↔`0xA0` dan `0xC8`↔`0xC0` di `sh1106.c` (fungsi `SH1106_Init`). |
| Baris terpotong / acak | Pastikan PB6/PB7 tidak dipakai peripheral lain (cocok dengan pinout master). |
| Kontras terlalu terang/gelap | Ubah nilai `0xCF` (contrast) di `SH1106_Init`. |

## Catatan

- Driver ini **tanpa library eksternal** (tidak perlu U8g2). Nanti untuk `ui.c`, `sh1106.c/h` bisa langsung dipakai ulang sebagai lapisan gambar.
- Kalau mau font berbeda/lebih besar, edit `Tools/gen_font.py` lalu jalankan (butuh Python + Pillow), salin hasilnya ke tabel `FONT6x8` di `sh1106.c`.
