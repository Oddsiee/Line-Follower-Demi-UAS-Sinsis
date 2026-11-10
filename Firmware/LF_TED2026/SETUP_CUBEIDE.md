# Setup STM32CubeIDE

Buat project baru bernama `LF_TED2026` di folder ini (board/MCU: **STM32F411CEUx**), simpan `LF_TED2026.ioc` di sini.

## Beri User Label sama dengan NetLabel PCB
CubeMX akan membuat `MUX_IN1_Pin`, `BTN_UP_Pin`, dst. di `main.h`.

| Pin | Fungsi | User Label |
|---|---|---|
| PA0, PA1, PA2, PB1 | ADC1_IN0, IN1, IN2, IN9 (scan + DMA, urutan Out1–Out4) | SENS_OUT1–4 |
| PA3, PB0 | GPIO Output (urutan In1, In2) | MUX_IN1, MUX_IN2 |
| PA8–PA11 | TIM1_CH1–CH4 PWM | DRV_D0–D3 |
| PB6, PB7 | I2C1 SCL/SDA | OLED_SCL, OLED_SDA |
| PA7, PA6, PA5, PA4 | GPIO Input, pull-up (urutan UP, DOWN, OK, BACK) | BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK |
| R (NRST) | Hardware reset, tombol ke GND | BTN_RST (net PCB; bukan GPIO) |
| PA15, PB3 | USART1 TX/RX (usulan debug) | DBG_TX, DBG_RX |

Catatan:
- Tabel ini memakai wiring terbaru. File `.ioc` (`Tes_Oled_Sensor_Baru.ioc`) sudah diperbarui ke pinout master, dan modul `Core` (sensor/motor/buttons/debug) sudah memakai pin baru. Regenerate kode CubeMX tetap diperlukan bila ingin memakai `main.c` hasil CubeMX; `Tests/test_sensor` sudah build + flash terverifikasi.
- ADC rank 1–4: kanal 0, 1, 2, 9. PA3 adalah output selektor mux, bukan input ADC; PB1 adalah input analog Out4. In1 dan In2 berada di dua port: GPIOA pin 3 dan GPIOB pin 0.
- USART1 di PA15/PB3 adalah **usulan**; pin ini tidak ada di pinout master tetapi kosong di header Black Pill (pin 10 dan 11). Pakai adaptor USB-UART 3.3 V. Jika dipakai, catat di dokumentasi master.
- TIM1: set ARR = 999 agar nilai compare = duty langsung (-1000..+1000 setelah di-map ke 0..1000).
- PA11 tetap `DRV_D3` (`TIM1_CH4`); USB data tidak digunakan bersamaan dengan PWM motor. Gunakan SWD untuk flash/debug.
- ADC: 12-bit, scan mode, DMA circular, sampling time cukup panjang (mulai 56 cycles).
- Debug: SWD aktif (PA13/PA14).

## Menghubungkan kode repo
Salin `Core/Inc/*.h` dan `Core/Src/*.c` dari repo ke project. Di `main.c`:
```c
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */
...
/* USER CODE BEGIN 2 */
App_Init();
/* USER CODE END 2 */
...
while (1) {
  /* USER CODE BEGIN 3 */
  App_Update();
  /* USER CODE END 3 */
}
```
