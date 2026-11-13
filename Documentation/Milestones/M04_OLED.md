# Milestone 4 — OLED & Tombol

- **Pemilik:** C
- **Jadwal:** 7–11 Nov
- **Status:** teruji di standalone `Tests/test_overall` (siap di-port ke `Firmware/LF_TED2026`)

## Tujuan
Menampilkan antarmuka menu dan status pada OLED SH1106 I2C1 (`0x3C`) serta membaca navigasi 4 tombol (`UP`, `DOWN`, `OK`, `BACK`) secara responsif tanpa glitch meskipun saat OLED sedang diperbarui.

## Sub-milestone
- [x] M4.1 Verifikasi driver SH1106 I2C1 (alamat `0x3C`, `SDA=PB7`, `SCL=PB6`) dan tampilkan teks/grafik (`Tests/test_overall`, catatan: driver standalone `sh1106.c`; port U8g2 ke `Firmware/LF_TED2026` menyusul)
- [x] M4.2 Baca tombol `UP/DOWN/OK/BACK` pada `PA7/PA6/PA5/PA4` (aktif LOW) dengan debounce `12 ms` dan long-press `500 ms` di `Tests/test_overall`; tombol HARD RESET ke `NRST` dan `GND`
- [x] M4.3 Kerangka menu interaktif & layar status teruji di `Tests/test_overall` (siap port ke `Core/Src/ui.c`)

## Selesai kalau
Menu bisa dinavigasi dengan 4 tombol tanpa glitch (tercapai di `Tests/test_overall`, tinggal port ke `Firmware/LF_TED2026`).

## Langkah yang dikerjakan
1. Perbaikan kabel power `VCC`/`GND` OLED pada hardware sehingga modul SH1106 menyala normal dengan suplai buck converter `3.3 V`.
2. Konfigurasi `I2C1` pada `100 kHz` (`ClockSpeed = 100000`) dan penambahan `HAL_Delay(150)` sebelum `SH1106_Init()`.
3. Pemindahan pembacaan tombol dari `main()` ke interrupt `1 kHz` (`SysTick_Handler` -> `Overall_Buttons_Tick1ms()`) dengan `BTN_DEBOUNCE_MS = 12U`, `BTN_LONGPRESS_MS = 500U`, dan FIFO 8-slot `s_evt_q[8]`.
4. Penerapan dirty-driven OLED refresh (`s_oled_dirty`, `last_oled = HAL_GetTick()` dicatat setelah `render_*()`, `400 ms` background refresh untuk layar statis dan `80 ms` untuk live sensor).

## Hasil uji
| Uji | Hasil | Catatan |
|---|---|---|
| OLED SH1106 (`100 kHz` + `150 ms` boot delay) | Lulus | Menyala normal saat ditenagai langsung dari buck converter `3.3 V` |
| Navigasi 4 tombol (`1 kHz` SysTick + FIFO 8-slot) | Lulus | `UP`, `DOWN`, `OK`, `BACK` sama responsifnya, tap cepat tidak ada yang terlewat |
| Menu & sub-halaman `test_overall` (Sensor, Motor, OLED/Power) | Lulus | Seluruh menu dapat dinavigasi tanpa glitch |

## Masalah & solusi
- **Masalah:** Sebelumnya OLED sempat gelap karena kabel power tertukar (kini sudah diperbaiki), dan di `400 kHz` I2C layar berisiko freeze/NACK karena `PB6/SCL` tidak punya pull-up eksternal (`g_i2c_pd = 0x80`).
  **Solusi:** Pastikan kabel power benar dan wajib gunakan **`100 kHz`** pada `I2C1` ditambah delay `150 ms` saat boot buck converter.
- **Masalah:** Polling tombol di `main()` terasa lambat/sering miss (terutama `OK`/`BACK`) karena `SH1106_Update()` di `100 kHz` memblokir `main()` ~`110 ms` per frame.
  **Solusi:** Sampling tombol di `SysTick_Handler` (`1 kHz`) masuk ke antrean FIFO 8-slot, plus dirty-driven OLED refresh.

## Catatan untuk milestone berikutnya
- Saat mengimplementasikan `Core/Src/buttons.c`, `Core/Src/ui.c`, dan `Core/Src/app.c` di `Firmware/LF_TED2026`, gunakan resep yang sama: `I2C1 = 100 kHz`, `HAL_Delay(150)` sebelum init OLED, polling tombol `1 kHz` di `SysTick_Handler` + debounce `12 ms` + long-press `500 ms` + FIFO event queue, serta dirty-driven OLED refresh (`400 ms` idle / `80 ms` live update).
