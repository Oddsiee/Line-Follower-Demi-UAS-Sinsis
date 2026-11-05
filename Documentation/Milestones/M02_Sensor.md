# Milestone 2 — Sensor 16CH

- **Pemilik:** A
- **Jadwal:** 7–11 Nov
- **Status:** 🟡 berjalan — hardware OK, baseline putih terukur

## Tujuan
Memastikan 16 kanal sensor terbaca stabil dan urutan fisik channel terverifikasi,
sebagai dasar kalibrasi hitam/putih dan perhitungan posisi garis.

## Sub-milestone
- [x] M2.1 Baca satu grup ADC (PA4); nilai berubah antara hitam dan putih
- [x] M2.2 Scan mux: set (In2, In1) 00→11, settling 5–10 us, baca PA4–PA7 menjadi raw[16]
- [x] M2.3 Optimasi: ADC scan mode + DMA; ukur waktu satu siklus scan penuh
- [x] M2.4 Tool test: cetak 16 nilai ke UART (lihat Tools/uart_plot.py)
- [ ] M2.5 Baseline kertas hitam + tentukan threshold per kanal
- [ ] M2.6 Verifikasi urutan fisik channel 0..15

## Selesai kalau
16 nilai stabil dan waktu scan terukur. Verifikasi urutan fisik channel.

## Langkah yang dikerjakan

### Firmware test standalone (`Tests/test_sensor/`)
Build + flash tanpa STM32CubeIDE, pola sama seperti `test_oled`:
```powershell
cd Tests\test_sensor
.\build.ps1      # -> build\test_sensor.elf
.\flash.ps1      # flash via ST-Link
.\read_sensor.ps1 -Samples 3 -IntervalMs 900   # baca via SWD
```

### Konfigurasi hardware
| Item | Setting |
|---|---|
| Clock | HSI 16 MHz (tanpa PLL) |
| ADC1 | Scan 4 kanal PA4–PA7, DMA2 Stream0, sampling 480 cycles |
| Mux | PB0 = In1, PB1 = In2 (output push-pull) |
| Settling mux | 50 us |
| Oversampling | 4x rata-rata per state |
| Kalibrasi ADC | manual via `ADC_CR2` bit CAL=2, RSTCAL=3 |

### Tool diagnostik
`build_diag.ps1` + `read_diag.ps1` — mengukur VREFINT (referensi internal),
membandingkan polling vs DMA, dan mengkarakterisasi tiap state mux.

## Hasil uji
| Uji | Hasil | Catatan |
|---|---|---|
| Build + flash | ✅ OK | text 15116, flash verified |
| Scan 16 kanal | ✅ OK | `scan_count` naik, `adc_err = 0` |
| Waktu scan penuh | ✅ **9.1 ms** | 4 mux × 4 kanal × oversample 4 |
| Stabilitas | ✅ OK | spread 9–77 setelah ground diperbaiki |
| Baseline putih | ✅ terukur | rentang 189–251, rata-rata ~222 |
| Baseline hitam | ⬜ belum | perlu diukur besok |
| Urutan fisik channel | ⬜ belum | perlu verifikasi dengan garis |

## Masalah & solusi

### 1. Firmware hang saat boot (`scan_count = 0`)
**Penyebab:** kalibrasi ADC ditulis ke bit 17/18 `ADC_CR2`, padahal itu
`JEXTSEL` yang tidak pernah clear sendiri → loop tunggu menggantung.
**Solusi:** bit yang benar `CAL`=2, `RSTCAL`=3. Ditambah timeout.

### 2. ST-Link `DEV_CONNECT_ERR` total
**Penyebab:** GND power supply sensor tidak disambung ke GND STM32.
**Solusi:** satukan ground. Probe juga perlu dicabut-colok USB.

### 3. Nilai sensor acak 0..950, saturasi 4095 di state 2/3
**Penyebab:** sama — ground tidak bersama. ADC mengukur terhadap ground
berbeda dari sinyal sensor.
**Solusi:** ground bersama → spread turun dari ~950 ke 9–77.

### 4. LED sensor tidak menyala
**Penyebab:** arus ST-Link (~100 mA) tidak cukup untuk 16 LED IR
(~160–320 mA).
**Solusi:** pakai power supply eksternal untuk sensor.

## Catatan untuk milestone berikutnya

- **Data lengkap ada di [`Documentation/Hardware/LOG_SENSOR.md`](../Hardware/LOG_SENSOR.md)**
- Nilai putih ~222 itu **rendah** — kemungkinan output modul *inverted*.
	Konfirmasi dengan kertas hitam sebelum menentukan threshold.
- `Sensor_ReadRaw()` di `Firmware/LF_TED2026/Core/Src/sensor.c` masih stub.
	Implementasi bisa langsung meniru `sensor_scan()` di `Tests/test_sensor/main.c`.
- Ground bersama adalah syarat mutlak. Tanpa itu semua pengukuran tidak valid.
