# Test OLED — Standalone (tanpa STM32CubeIDE)

Firmware test OLED SH1106 yang bisa di-build & flash **langsung dari PowerShell**.
Tidak perlu buka CubeIDE sama sekali.

## Cara pakai (2 perintah)

```powershell
cd Tests\test_oled
.\build.ps1     # compile + link -> build\test_oled.elf
.\flash.ps1     # flash ke STM32 via ST-Link
```

Selesai. OLED langsung menampilkan layar demo.

## Isi folder

| File | Fungsi |
|---|---|
| `main.c` | Entry point: clock HSI 16 MHz, I2C1 PB6/PB7 400 kHz, panggil `TestOled_Init()` + `TestOled_Run()` |
| `stm32f4xx_hal_conf.h` | Konfigurasi HAL minimal (RCC, GPIO, CORTEX, I2C, DMA, FLASH, PWR) |
| `stm32f4xx_it.c` | SysTick handler (`HAL_IncTick`) + fault handlers |
| `STM32F411CEUX_FLASH.ld` | Linker script: 512K FLASH @ 0x08000000, 128K RAM @ 0x20000000 |
| `build.ps1` | Compile semua sumber + link jadi `.elf`/`.hex`/`.bin` |
| `flash.ps1` | Flash `.elf` ke board via ST-Link + verify + reset |

Driver OLED-nya **tidak diduplikasi** — `build.ps1` mengambil langsung dari:
```
Firmware\LF_TED2026\Core\Src\sh1106.c
Firmware\LF_TED2026\Core\Src\test_oled.c
Firmware\LF_TED2026\Core\Inc\sh1106.h
Firmware\LF_TED2026\Core\Inc\test_oled.h
```
Jadi kalau driver diperbaiki, cukup `.\build.ps1` lagi.

## Prasyarat

1. **ARM GCC** — bawaan STM32CubeIDE (`C:\ST\STM32CubeIDE_2.2.0\...\gnu-tools-for-stm32...\bin`).
   `build.ps1` mencari otomatis kalau path default berubah.
2. **Firmware package** `STM32Cube_FW_F4_V1.28.3` di
   `C:\Users\<kamu>\STM32Cube\Repository\` (lihat `Firmware/LF_TED2026/TEST_OLED.md` bagian 0).
3. **STM32CubeProgrammer** di `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\`.

## Wiring

| OLED | Black Pill |
|---|---|
| VCC | 3V3 (pin 20) |
| GND | GND (pin 19) |
| SCL | PB6 |
| SDA | PB7 |

| ST-Link | Black Pill |
|---|---|
| 3.3V | 3V3 |
| GND | GND |
| SWDIO | PA13 |
| SWCLK | PA14 |

## Hasil build (terverifikasi)

```
   text    data     bss     dec     hex
  11820     104    3024   14948    3a64
```

Flash terverifikasi: vector table di `0x08000000` berisi
`SP=0x20020000`, `Reset_Handler=0x080020C5`.

## Layar yang muncul (ganti tiap 2 detik)

1. **Boot** — `SH1106 OLED TEST` / `Line Follower` / `TED 2026 - SV UGM` / `PB6/PB7 I2C1`
2. **I2C scan** — `OLED @ 0x3C OK` (atau `0x3D`, atau `TIDAK TERDETEKSI`)
3. **Shapes** — lingkaran, kotak, garis
4. **Live simulasi** — 16 batang sensor + marker bergerak

## Troubleshooting

| Gejala | Solusi |
|---|---|
| Layar gelap total | Lihat layar ke-2 (I2C scan). Kalau `TIDAK TERDETEKSI`, cek kabel SCL/SDA/VCC/GND. |
| `No ST-Link detected` | Cek kabel USB data, driver ST-Link, ganti port USB. |
| `Gagal compile` | Cek firmware package ada di `STM32Cube\Repository`. |
| Gambar terbalik | Tukar `0xA1`↔`0xA0` dan `0xC8`↔`0xC0` di `sh1106.c` (`SH1106_Init`). |
| Terdeteksi di `0x3D` | Sudah otomatis — `TestOled_Init()` scan dua alamat. |
| Warning `_close/_read not implemented` | Normal, dari `--specs=nosys.specs`. Abaikan. |
