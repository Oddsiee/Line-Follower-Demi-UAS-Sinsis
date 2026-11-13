# Test Motor Driver ZK-5AD — Standalone (tanpa STM32CubeIDE)

Firmware uji untuk memastikan **koneksi STM32 ↔ driver motor ZK-5AD** sehat,
sebelum masuk Milestone 3 (`motor.c`) dan tuning PID.

Bisa di-build & flash **langsung dari PowerShell**.

## Cara pakai (3 perintah)

```powershell
cd Tests\test_motor
.\build.ps1     # compile + link -> build\test_motor.elf
.\flash.ps1     # flash ke STM32 via ST-Link
.\read_motor.ps1 -Samples 5 -IntervalMs 500   # baca status via SWD (tanpa reset)
```

> ⚠️ **Angkat/ganjal dulu kedua roda robot sebelum `flash.ps1`.** Setelah reset,
> firmware langsung menjalankan fase uji otomatis dan motor akan berputar.

## Wiring

| Driver ZK-5AD | Net | Black Pill | Fungsi |
|---|---|---|---|
| Pin 1 `GND` | GND | **G** | Ground sinyal PWM (**WAJIB**) |
| Pin 2 `3V3` | — | **NC** | **JANGAN disambung!** (output 3V3 internal driver) |
| Pin 3 `D0` | DRV_D0 | **PA8** | `TIM1_CH1` — Motor kiri IN1 |
| Pin 4 `D1` | DRV_D1 | **PA9** | `TIM1_CH2` — Motor kiri IN2 |
| Pin 5 `D2` | DRV_D2 | **PA10** | `TIM1_CH3` — Motor kanan IN1 |
| Pin 6 `D3` | DRV_D3 | **PA11** | `TIM1_CH4` — Motor kanan IN2 |
| Pin 7 `VT` | — | **NC** | Tidak digunakan |

Daya motor (`+12V_SW` + `GND` baut) dari XT60/saklar utama ke terminal baut driver.

OLED opsional (kalau terpasang): `PB6 = SCL`, `PB7 = SDA`, `VCC -> 3V3`, `GND -> GND`.

## Fase uji otomatis (ganti tiap 1,5 detik)

| # | Fase | Yang dilakukan | Kegunaan |
|---|---|---|---|
| 1 | `COAST bebas` | D0..D3 semua LOW | referensi motor bebas |
| 2 | `PIN D0 kiri+` | hanya PA8 100% duty | uji koneksi pin D0 |
| 3 | `PIN D1 kiri-` | hanya PA9 100% duty | uji koneksi pin D1 |
| 4 | `PIN D2 kanan+` | hanya PA10 100% duty | uji koneksi pin D2 |
| 5 | `PIN D3 kanan-` | hanya PA11 100% duty | uji koneksi pin D3 |
| 6 | `MAJU pelan` | `Motor_Set(300, 300)` | kedua roda maju |
| 7 | `MAJU cepat` | `Motor_Set(700, 700)` | |
| 8 | `KIRI saja` | `Motor_Set(600, 0)` | roda kiri saja |
| 9 | `KANAN saja` | `Motor_Set(0, 600)` | roda kanan saja |
| 10 | `MUNDUR pelan` | `Motor_Set(-300, -300)` | arah mundur |
| 11 | `REM (brake)` | kedua pin HIGH | rem aktif |

**Fase 2–5 (pin walk)** adalah uji koneksi paling langsung: hanya **satu** pin yang
aktif, jadi kalau pakai multimeter bisa ukur D0..D3 satu per satu terhadap GND
(tegangan rata-rata = duty × logika; 100% duty → mendekati VCC logika driver ≈ 3,3 V).

Tombol (opsional): **OK (PA5)** = fase berikutnya, **BACK (PA4)** = jeda/lanjut auto.
LED onboard (PC13): nyala saat fase REM.

## Logika driver (ZK-5AD)

```
Maju   : IN1 = PWM, IN2 = LOW
Mundur : IN1 = LOW, IN2 = PWM
Rem    : IN1 = HIGH, IN2 = HIGH
Coast  : IN1 = LOW,  IN2 = LOW
```

PWM: `TIM1`, `PSC=0`, `ARR=999` → **16 kHz** @ clock HSI 16 MHz.
Nilai compare `0..999` setara duty `0..100%` (skala `Motor_Set` = −1000..+1000).

## Yang dibaca `read_motor.ps1`

- Variabel firmware: `g_phase`, `g_phase_count`, `g_loop`, `g_cmd_left`, `g_cmd_right`,
  `g_oled_ok`, `g_paused`, `g_btn_ok`, `g_btn_back`, `g_moe_on`.
- Register `TIM1`: `CR1` (CEN), `CCER` (CCxE), `CNT`, `CCR1..CCR4`, `BDTR` (MOE).
- `RCC_APB2ENR` (bit0 = TIM1EN) + `GPIOA_MODER` + `GPIOA_AFRH` (PA8..PA11).

Dipakai mode `hotplug` → **MCU tidak di-reset**, nilai terlihat live.

## Cek cepat kalau motor tidak bergerak

1. **`CCR1..CCR4` di `read_motor.ps1` berubah sesuai fase?**
   – Kalau ya, MCU sudah benar mengeluarkan PWM. Masalah di sisi driver/wiring/daya.
   – Kalau tidak, lihat poin 2–4.
2. **`RCC_APB2ENR TIM1EN = 1`** dan **`BDTR MOE = 1`** dan **`CR1 CEN = 1`**?
   – Kalau `MOE = 0`, TIM1 (advanced timer) tidak mengeluarkan sinyal di pin.
3. **`GPIOA_MODER PA8..PA11 = 10` (AF) dan `AFRH = 1` (TIM1)**?
4. **Driver dapat daya?** LED power driver nyala, `+12V_SW` hadir di terminal baut.
5. **GND bersama?** GND driver (pin 1) WAJIB tersambung ke GND board/ST-Link.
   Ini penyebab paling umum "PWM bagus tapi motor diam".
6. **Pin 2 (3V3) driver jangan tersambung** — kalau tersambung ke `+3V3_SYS`,
   bisa tabrakan regulator.
7. Roda kiri/kanan **tertukar**? Tukar pemetaan `Motor_Set` atau tukar pasangan
   pin D0/D1 ↔ D2/D3 di `motor.c` (bukan di test ini).

## Isi folder

| File | Fungsi |
|---|---|
| `main.c` | Clock HSI 16 MHz, TIM1 PWM 4 kanal, mesin fase uji, OLED + tombol |
| `stm32f4xx_hal_conf.h` | Konfigurasi HAL minimal (RCC, GPIO, CORTEX, DMA, I2C, TIM, FLASH, PWR) |
| `stm32f4xx_it.c` | SysTick (`HAL_IncTick`) + fault handlers |
| `STM32F411CEUX_FLASH.ld` | Linker script: 512K FLASH @ 0x08000000, 128K RAM @ 0x20000000 |
| `build.ps1` | Compile + link → `.elf`/`.hex`/`.bin` |
| `flash.ps1` | Flash + verify + reset via ST-Link |
| `read_motor.ps1` | Baca variabel `g_*` + register TIM1/GPIO via SWD |

Driver motor (`motor.c`) dan OLED (`sh1106.c`) **tidak diduplikasi** — `build.ps1`
mengambil langsung dari `Firmware\LF_TED2026\Core\Src\`. Jadi kalau `motor.c`
diperbaiki, cukup `.\build.ps1` lagi.

## Prasyarat

1. **ARM GCC** bawaan STM32CubeIDE (`C:\ST\STM32CubeIDE_2.2.0\...`).
2. **Firmware package** `STM32Cube_FW_F4_V1.28.3` di `C:\Users\<kamu>\STM32Cube\Repository\`.
3. **STM32CubeProgrammer** di `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\`.

## Catatan Milestone 3

Test ini memakai `motor.c` asli dari `Core` (`Motor_Init/Set/Brake/Coast`) + `config.h`
(`MOTOR_PWM_ARR=999`). Jadi kalau `build.ps1` lulus dan fase 6–11 menggerakkan motor
dengan arah benar, sebagian besar checklist M3.1–M3.3 sudah terbukti di perangkat keras.
M3.4 (uji roda terangkat, arah benar, kiri/kanan tidak tertukar) bisa dilakukan langsung
dari fase `MAJU/KIRI/KANAN/MUNDUR`.
