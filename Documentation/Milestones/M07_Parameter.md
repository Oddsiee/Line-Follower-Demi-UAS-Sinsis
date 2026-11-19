# Milestone 7 — Parameter & Penyimpanan

> **ATURAN MUTLAK LOKASI CODING UTAMA:** Seluruh codingan utama robot Line Follower **HANYA** ditulis pada folder **`Line Follower Coding Utama/`** (tempat `.ioc` utama `Line Follower Coding Utama/Line Follower Coding Utama.ioc` berada, MCU **`STM32F401CCU6`**) dan **TIDAK ADA coding untuk robot Line Follower di luar folder ini**.

- **Pemilik:** C
- **Jadwal:** 12–16 Nov
- **Status:** belum mulai

## Tujuan
(isi satu-dua kalimat)

## Sub-milestone
- [ ] M7.1 Layar edit Kp, Ki, Kd, base speed, max speed
- [ ] M7.2 Simpan Params & kalibrasi ke sektor Flash terakhir (sektor 5 = `0x08020000` pada `STM32F411CCU6` 256 KB; sektor 7 tidak ada di varian 256 KB); tulis hanya saat IDLE
- [ ] M7.3 Muat otomatis saat boot, validasi magic number / checksum
- [ ] M7.4 State machine: IDLE -> CALIB -> READY -> RUN -> STOP

## Selesai kalau
Parameter tetap tersimpan setelah power off.

## Langkah yang dikerjakan
(catat langkah penting, setting CubeMX, library yang dipakai)

## Hasil uji
| Uji | Hasil | Catatan |
|---|---|---|
| | | |

## Masalah & solusi
- 

## Catatan untuk milestone berikutnya
- 
