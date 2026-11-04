# Setup STM32CubeIDE

Buat project baru bernama `LF_TED2026` di folder ini (board/MCU: **STM32F411CEUx**), simpan `LF_TED2026.ioc` di sini.

## Beri User Label sama dengan NetLabel PCB
CubeMX akan membuat `MUX_IN1_Pin`, `BTN_UP_Pin`, dst. di `main.h`.

| Pin | Fungsi | User Label |
|---|---|---|
| PA4–PA7 | ADC1_IN4–IN7 (scan + DMA) | SENS_OUT1–4 |
| PB0, PB1 | GPIO Output | MUX_IN1, MUX_IN2 |
| PA8–PA11 | TIM1_CH1–CH4 PWM | DRV_D0–D3 |
| PB6, PB7 | I2C1 SCL/SDA | OLED_SCL, OLED_SDA |
| PA0–PA3 | GPIO Input, pull-up | BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK |
| PA15, PB3 | USART1 TX/RX (usulan debug) | DBG_TX, DBG_RX |

Catatan:
- USART1 di PA15/PB3 adalah **usulan**; pin ini tidak ada di pinout master tetapi kosong di header Black Pill (pin 10 dan 11). Pakai adaptor USB-UART 3.3 V. Jika dipakai, catat di dokumentasi master.
- TIM1: set ARR = 999 agar nilai compare = duty langsung (-1000..+1000 setelah di-map ke 0..1000).
- PA11 juga pin USB; lihat keputusan di Milestone 1.
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
