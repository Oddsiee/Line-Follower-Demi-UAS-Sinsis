# Test Sensor 16CH Dariyan-X — Standalone

Firmware uji sensor 16 channel yang bisa di-build & flash **langsung dari PowerShell**.
Tidak perlu buka STM32CubeIDE.

> 📊 **Data pengukuran nyata** (baseline putih, karakterisasi mux, VREFINT):
> [`Documentation/Hardware/LOG_SENSOR.md`](../../Documentation/Hardware/LOG_SENSOR.md)

> **Wiring terbaru:** tabel di bawah memakai Out1–Out4 pada PA0/PA1/PA2/PB1 (`ADC1_IN0/IN1/IN2/IN9`), In1=PA3, dan In2=PB0. Firmware `main.c` dan firmware diagnostik masih perlu disesuaikan pada inisialisasi GPIO analog, kanal/rank ADC, serta GPIO selektor mux sebelum build dan flash untuk wiring ini. Hasil build serta pengukuran yang dicatat di sini berasal dari pengujian sebelumnya; uji ulang setelah penyesuaian pin.

## Cara pakai

```powershell
cd Tests\test_sensor
.\build.ps1        # compile + link -> build\test_sensor.elf
.\flash.ps1        # flash ke STM32 via ST-Link
.\read_sensor.ps1  # baca 16 nilai via SWD (tanpa UART)
```

Baca berulang (pantau saat sensor digeser di atas garis):

```powershell
.\read_sensor.ps1 -Samples 20 -IntervalMs 500
```

## Wiring — URUTAN PIN SENSOR KE STM32

Konektor sensor 1x8 pin. Urutan sesuai pinout master:

Pemetaan pin STM32: **PA0, PA1, PA2, PA3, PB0, PB1** = **Out1, Out2, Out3, In1, In2, Out4**.

| Pin sensor | Nama | Sambung ke STM32 | Fungsi |
|---|---|---|---|
| **1** | `Out1` | **PA0** | ADC1_IN0 — grup sensor 1–4 |
| **2** | `Gnd` | **GND** | Ground |
| **3** | `Out2` | **PA1** | ADC1_IN1 — grup sensor 5–8 |
| **4** | `Out3` | **PA2** | ADC1_IN2 — grup sensor 9–12 |
| **5** | `In1` | **PA3** | Selektor mux bit 0 |
| **6** | `In2` | **PB0** | Selektor mux bit 1 |
| **7** | `Vcc` | **3V3** | Daya sensor +3.3 V |
| **8** | `Out4` | **PB1** | ADC1_IN9 — grup sensor 13–16 |

> **Penting:** pin 2 (Gnd) ada di TENGAH konektor, bukan di ujung.
> Jangan tertukar dengan urutan "Out1..Out4 lalu Gnd". Ikuti tabel di atas.

### Cara kerja mux

Sensor punya 16 fotodioda tapi hanya 4 output analog. Multiplexer memilih
4 kanal sekaligus:

| In2 (PB0) | In1 (PA3) | Yang terbaca di Out1..Out4 |
|---|---|---|
| 0 | 0 | sensor 1, 5, 9, 13 |
| 0 | 1 | sensor 2, 6, 10, 14 |
| 1 | 0 | sensor 3, 7, 11, 15 |
| 1 | 1 | sensor 4, 8, 12, 16 |

In1 berada pada GPIOA pin 3 dan In2 pada GPIOB pin 0. Tulis kedua port sebelum jeda settling. Rank ADC 1–4 harus mengikuti kanal **0, 1, 2, 9** untuk Out1–Out4.

Firmware menyimpan hasil ke `raw[group*4 + mux_state]`, jadi indeks 0..15
adalah urutan fisik sensor setelah diverifikasi.

## UART (opsional)

Kalau ada adaptor USB-UART 3.3 V:

| Adaptor | STM32 |
|---|---|
| RX | PA15 (TX) |
| TX | PB3 (RX) |
| GND | GND |

Lalu plot real-time:

```powershell
python ..\..\Tools\uart_plot.py COM5 115200
```

Format baris: `S,v0,v1,...,v15,pos`

## OLED (opsional)

Kalau OLED terpasang dengan SDA ke PB7 dan SCL ke PB6 (I2C1), layar menampilkan bar 16 kanal + tabel nilai.
Kalau tidak terpasang, firmware tetap jalan (driver timeout, tidak hang).

## Isi folder

| File | Fungsi |
|---|---|
| `main.c` | Clock HSI 16 MHz, ADC1 scan 4 kanal + DMA, OLED, UART; target mux In1=PA3/In2=PB0 memerlukan penyesuaian firmware |
| `stm32f4xx_hal_conf.h` | HAL minimal: RCC, GPIO, CORTEX, ADC, DMA, I2C, UART, FLASH, PWR |
| `stm32f4xx_it.c` | SysTick + DMA2_Stream0 (ADC) |
| `STM32F411CEUX_FLASH.ld` | Linker script 512K FLASH / 128K RAM |
| `build.ps1` | Compile + link |
| `flash.ps1` | Flash via ST-Link |
| `read_sensor.ps1` | Baca `g_raw[]` via SWD (mode hotplug, tidak reset MCU) |

Driver OLED diambil langsung dari `Firmware/LF_TED2026/Core/Src/sh1106.c`.

## Hasil build (terverifikasi)

```
   text    data     bss     dec     hex
  14936     100    3388   18424    47f8
```

## Hasil uji

| Uji | Hasil | Catatan |
|---|---|---|
| Build + flash | OK | text 14936, flash verified |
| Scan berjalan | OK | `scan_count` naik terus, `adc_err = 0` |
| Waktu scan | **9.1 ms** | 4 mux x 4 kanal x oversample 4, sampling 480 cycles |
| Nilai stabil | **OK** | setelah GND power supply disambung ke GND STM32 |

### Baseline kertas PUTIH (2026-11-06)

Semua 16 sensor di atas kertas putih, ground bersama:

| Kanal | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Nilai | 240 | 243 | 231 | 229 | 222 | 226 | 226 | 219 |

| Kanal | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|
| Nilai | 245 | 251 | 196 | 189 | 228 | 232 | 202 | 196 |

- Rentang: **~190–260**
- Variasi antar sampel: **±10–20** (stabil)
- Saturasi: **0/16**

## Troubleshooting

| Gejala | Kemungkinan penyebab |
|---|---|
| Semua nilai 0 | Sensor tidak dapat daya (pin 7 Vcc), atau GND lepas |
| Nilai acak 0..4095 tidak stabil | Pin analog floating → Out1..Out4 belum tersambung |
| Nilai sama semua 16 kanal | In1/In2 (PA3/PB0) tidak tersambung atau GPIO selektor belum disesuaikan → mux tidak berganti |
| `scan_count = 0` | Firmware tidak jalan / board belum di-flash |
| `adc_err > 0` | Timeout DMA — ADC tidak selesai konversi |
| **Nilai acak 0..950, mean ~350, state 2/3 saturasi 4095** | **GND power supply TIDAK disambung ke GND STM32** |

### WAJIB: ground bersama

Kalau sensor diberi daya dari **power supply eksternal**, GND-nya **harus**
disambung ke GND STM32. Tanpa itu, ADC mengukur terhadap ground yang berbeda
dan hasilnya angka acak yang tidak berarti.

```
Power Supply (−) ──┬──► GND sensor (pin 2)
          └──► GND STM32 (Black Pill)
            └──► GND ST-Link
```

**Cara mendiagnosis:** jalankan `.\build_diag.ps1` lalu `.\read_diag.ps1`.

| Hasil | Arti |
|---|---|
| VREFINT spread kecil (<20), output sensor spread besar | Ground tidak bersama / sensor floating |
| VREFINT spread besar | Suplai MCU (Vdda) tidak stabil |
| Polling stabil tapi DMA kacau | Bug jalur DMA |

VREFINT normal ~1500 @ Vdda 3.3 V. Nilai ini **tidak tergantung sensor**,
jadi bisa dipakai memisahkan masalah ADC vs masalah wiring sensor.

## Catatan teknis

- **Kalibrasi ADC manual.** Paket HAL/CMSIS di mesin ini tidak punya
  `HAL_ADCEx_Calibration_Start`, jadi bit `ADC_CR2` ditulis langsung:
  `ADON`(0) → `RSTCAL`(3) → `CAL`(2). **Jangan pakai bit 17/18** — itu
  `JEXTSEL`, dan loop tunggunya akan hang.
- Sampling time 480 cycles @ ADC clock 4 MHz (PCLK2 16 MHz / 4) = 30 µs/kanal.
- Settling mux 10 µs sebelum tiap konversi.
- `read_sensor.ps1` pakai `-c port=SWD mode=hotplug` supaya MCU tidak reset
  saat dibaca (setiap `-c` biasa akan reset).
