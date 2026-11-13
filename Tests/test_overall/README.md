# Test Overall Hardware (Standalone)

Firmware pengujian terpadu untuk memverifikasi seluruh hardware robot secara mandiri menggunakan suplai baterai / buck converter tanpa perlu terhubung ke ST-Link atau PC.

## Fitur & Pengamanan Power-On
- **Settling Delay 150 ms** saat boot agar tegangan 3.3V dari buck converter stabil sebelum inisialisasi OLED I2C.
- **Motor Default COAST (Diam)**: roda dijamin tidak berputar saat baru dinyalakan maupun saat berpindah menu.
- **LED Heartbeat `PC13`**: berkedip setiap 500 ms saat standby, dan 100 ms saat motor aktif (`[RUN!]`).

## Pin yang Diuji
| Modul | Pin STM32 | Keterangan |
|---|---|---|
| Tombol | `PA7` (UP), `PA6` (DOWN), `PA5` (OK), `PA4` (BACK) | Input pull-up, aktif LOW |
| OLED SH1106 | `PB6` (SCL), `PB7` (SDA) | I2C1 400 kHz, auto-probe `0x3C`/`0x3D` |
| Sensor 16CH | `PA0`, `PA1`, `PA2`, `PB1` (ADC) + `PA3`, `PB0` (Mux) | ADC1 Ch 0, 1, 2, 9 + VREFINT via DMA |
| Motor ZK-5AD | `PA8` (D0), `PA9` (D1), `PA10` (D2), `PA11` (D3) | TIM1 CH1–CH4 PWM 16 kHz |

## Mode Pengujian & Kontrol Tombol

Di **Menu Utama**:
- `UP` / `DOWN`: Geser kursor (`1..4`)
- `OK`: Masuk ke mode uji yang dipilih

### 1. Test Tombol
- Menampilkan status *real-time* 4 kotak (`UP`, `DN`, `OK`, `BK`) dan counter jumlah penekanan.
- Tekan singkat `UP`/`DOWN`/`OK`/`BACK` untuk menguji respon & debounce.
- **Tahan `BACK` (≥ 0.8 detik)** untuk kembali ke Menu Utama.

### 2. Test Sensor 16CH
- `UP` / `DOWN`: Pindah halaman (`1/3` Bar Graph + Posisi, `2/3` Raw CH 0–7, `3/3` Raw CH 8–15).
- `OK`: Toggle `HOLD` (bekukan tampilan angka).
- `BACK`: Kembali ke Menu Utama.

### 3. Test Motor ZK-5AD
- Selalu mulai dalam status **`[STOP]`** (aman).
- `UP` / `DOWN`: Pilih fase uji (otomatis `STOP` tiap ganti fase).
- `OK`: Toggle **`[RUN!]` / `[STOP]`** untuk memutar/menghentikan motor pada fase terpilih.
- `BACK`: Langsung matikan motor (`Motor_Coast()`) dan kembali ke Menu Utama.
- Memantau live `Vdda` (mV) di layar untuk mendeteksi drop tegangan buck converter saat motor berputar.

### 4. Test OLED & Power
- `UP` / `DOWN`: Pindah halaman (`1/3` Monitor tegangan 3.3V `Vdda`, Min/Max/Ripple & statistik I2C, `2/3` Pola Checkerboard, `3/3` Layar Putih Penuh).
- `OK`: Reset statistik `Min`/`Max` tegangan di halaman `1/3`, atau kembali ke halaman `1/3` dari pola uji.
- `BACK`: Kembali ke Menu Utama.

## Build & Flash
```powershell
cd Tests\test_overall
.\build.ps1
.\flash.ps1
```

## Catatan Teknis & Konfigurasi Terbaik (Hasil Uji 2026-11-13)

Konfigurasi berikut sudah terbukti stabil dan responsif pada hardware final (didaya langsung dari buck converter `3.3 V`) dan wajib menjadi acuan saat mem-porting ke firmware utama (`Firmware/LF_TED2026`):

1. **Konfigurasi I2C1 & Power-On OLED (`SH1106`):**
   - `hi2c1.Init.ClockSpeed = 100000` (**wajib `100 kHz`**, bukan `400 kHz`): pin `PB6/SCL` mengandalkan internal pull-up STM32 (~40 kOhm; terukur `g_i2c_pd = 0x80`), sehingga `400 kHz` berisiko NACK dan membuat layar freeze.
   - Tambahkan settling delay **`BOOT_SETTLE_MS = 150U`** (`HAL_Delay(150)`) sebelum `SH1106_Init()` agar suplai `3.3 V` dari buck converter `MP1584EN` benar-benar stabil.

2. **Resep Responsivitas Tombol (Anti-Miss Saat OLED Sibuk):**
   - Pada `100 kHz` I2C, satu kali `SH1106_Update()` memblokir `main()` selama **~110 ms**. Polling tombol di `main()` atau refresh OLED berkala setiap `80 ms` akan membuat tap singkat (`50–100 ms`) sering terlewat.
   - **Solusi teruji:**
     - **Sampling `1 kHz` di `SysTick_Handler`** (`Overall_Buttons_Tick1ms()` di `stm32f4xx_it.c`) agar status pin `PA7/PA6/PA5/PA4` selalu terbaca setiap `1 ms` meskipun `main()` sedang menunggu transfer I2C OLED.
     - **Debounce `12 ms` (`BTN_DEBOUNCE_MS = 12U`)** dan **Long-press `500 ms` (`BTN_LONGPRESS_MS = 500U`)**.
     - **Antrean event FIFO lock-free 8 slot (`s_evt_q[8]`)** untuk menampung urutan klik cepat (mis. `DOWN -> OK`) selama frame OLED sedang dikirim.

3. **Strategi Refresh OLED (Dirty-Driven):**
   - Gunakan flag `s_oled_dirty = 1` setiap kali ada event tombol atau perpindahan mode/halaman agar perubahan UI langsung digambar tanpa menunggu interval.
   - Catat timestamp **`last_oled = HAL_GetTick()` setelah `render_*()` selesai** (bukan sebelum render), sehingga masih ada jeda bus I2C antar-frame.
   - Gunakan **`OLED_BG_REFRESH_MS = 400U`** untuk halaman menu/statis dan **`OLED_SENSOR_MS = 80U`** khusus untuk grafik sensor live.
