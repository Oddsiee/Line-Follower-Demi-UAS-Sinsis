# Line Follower — TED 2026 (SV UGM)

Robot line follower tim S1 Teknik Elektro Universitas Diponegoro untuk **Tech Enthusiast Day 2026**, Sekolah Vokasi UGM. Lomba: **28 November 2026**.

Firmware ditulis di **STM32CubeIDE** untuk STM32F411CEU6 (Black Pill).

## Hardware ringkas
| Bagian | Komponen |
|---|---|
| MCU | STM32F411CEU6 "Black Pill V3" |
| Sensor | Dariyan-X 16CH (dual 4-ch mux, 4 output analog) |
| Driver | ZK-5AD dual H-bridge |
| Layar | OLED 1.3" I2C, chip **SH1106** |
| Daya | LiPo 3S 11.1 V, buck MP1584EN 3.3 V |

Detail lengkap, pinout, dan aturan PCB: [dokumentasi master](dokumentasi_master_robot_line_follower_ted_2026.md).

### Wiring terbaru

| Bagian | Pin STM32 dan sinyal |
|---|---|
| Sensor | PA0 = Out1, PA1 = Out2, PA2 = Out3, PA3 = In1, PB0 = In2, PB1 = Out4 |
| Tombol | PA7 = UP, PA6 = DOWN, PA5 = OK, PA4 = BACK, R (NRST) = HARD RESET |
| OLED | PB7 = SDA, PB6 = SCL (I2C1) |
| Driver motor | PA8 = D0, PA9 = D1, PA10 = D2, PA11 = D3 (TIM1_CH1–CH4) |

Konfigurasi `.ioc` (`Firmware/LF_TED2026/Tes_Oled_Sensor_Baru.ioc`) dan firmware sensor/tombol sudah disesuaikan dengan pin baru. `Tests/test_sensor` terverifikasi build + flash (scan berjalan, `adc_err` = 0).

## Status progres
| Modul | Status | Catatan |
|---|---|---|
| OLED SH1106 | 🟢 hardware & I2C OK | kabel power OLED sudah diperbaiki; wajib `100 kHz` I2C1 + delay boot `150 ms` |
| Tombol Menu (4-button) | 🟢 teruji responsif | polling `1 kHz` di `SysTick` + debounce `12 ms` + long-press `500 ms` + antrean FIFO 8-slot |
| Sensor 16CH | 🟢 teruji via overall | pembacaan mux 16CH terverifikasi di menu grafik/tabel `test_overall` |
| Motor ZK-5AD | 🟢 teruji via overall | 4 kanal PWM `TIM1` (kiri/kanan, maju/mundur/coast/brake) terverifikasi |
| Uji Overall & Buck 3.3V | 🟢 lulus | seluruh subsistem berjalan mandiri dari buck converter via `Tests/test_overall` |

**Data pengukuran sensor:** `Documentation/Hardware/LOG_SENSOR.md`

## Struktur proyek
```
Firmware/LF_TED2026/   Project STM32CubeIDE (.ioc, Core/Inc, Core/Src)
Tools/                 Skrip bantu (plot UART, analisis log)
Tests/                 Firmware uji per modul (test_oled, test_sensor, test_motor, test_overall)
Documentation/         PROJECT.md, Hardware/, Milestones/
```

## Tim & kepemilikan modul
| Peran | Nama | Modul |
|---|---|---|
| A — Sensing & Kontrol | (isi) | sensor, calibration, pid |
| B — Motor & Hardware | (isi) | motor, pengukuran, mekanik, lintasan uji |
| C — UI & Sistem | (isi) | ui, buttons, storage, track, app |

Satu modul, satu pemilik. Ubah modul orang lain lewat issue/chat, bukan edit langsung.

## Aturan Git
- Branch per orang: `feat/sensor`, `feat/motor`, `feat/ui`. Merge ke `main` di akhir tiap fase.
- `.ioc` hanya diubah satu orang pada satu waktu; kabari grup dulu.
- Di file hasil generate CubeMX, tulis kode hanya di blok `USER CODE BEGIN/END`.
- Commit: `[M2] sensor: scan mux 16 channel`.

## Progress
- [ ] Milestone 0 — Perencanaan & Kontrak Antarmuka (Semua, 4–6 Nov)
- [x] Milestone 1 — Bring-up Hardware (Semua, 4–6 Nov) — terverifikasi via `Tests/test_overall` + buck `3.3 V`
- [x] Milestone 2 — Sensor 16CH (A, 7–11 Nov) — hardware & mux terverifikasi via `Tests/test_overall`
- [x] Milestone 3 — Driver Motor (B, 7–11 Nov) — 4 kanal PWM `TIM1` terverifikasi via `Tests/test_overall`
- [x] Milestone 4 — OLED & Tombol (C, 7–11 Nov) — terverifikasi via `Tests/test_overall` (siap port ke `Firmware/LF_TED2026`)
- [ ] Milestone 5 — Kalibrasi & Posisi Garis (A, 12–16 Nov)
- [ ] Milestone 6 — Karakterisasi Motor (B, 12–16 Nov)
- [ ] Milestone 7 — Parameter & Penyimpanan (C, 12–16 Nov)
- [ ] Milestone 8 — Integrasi PID (A (B, C mendukung), 17–21 Nov)
- [ ] Milestone 9 — Logika Lintasan (C (A mendukung, B lintasan uji), 17–21 Nov)
- [ ] Milestone 10 — Tuning Lintasan (Semua, 22–25 Nov)
- [ ] Milestone 11 — Pengujian & Regulasi (Semua, 25–26 Nov)
- [ ] Milestone 12 — Dokumentasi (Semua (paralel), Sepanjang proyek)

## Gate
- **11 Nov:** sensor terbaca, motor bergerak, menu berjalan (masing-masing berdiri sendiri)
- **16 Nov:** tiap modul lulus tes sendiri
- **21 Nov:** satu putaran lintasan lengkap berhasil, minimal pelan
- **27 Nov:** freeze firmware

## Build & flash
1. Buat project STM32CubeIDE sesuai `Firmware/LF_TED2026/SETUP_CUBEIDE.md`
2. Salin `Core/Inc` dan `Core/Src` dari repo ke project, lalu panggil `App_Init()` / `App_Update()` dari `main.c`
3. Flash via ST-Link (SWD)
