# Milestone 0 — Perencanaan & Kontrak Antarmuka

- **Pemilik:** Semua
- **Jadwal:** 4–6 Nov
- **Status:** berjalan — kontrak dan aturan kerja ditetapkan; verifikasi clone-build per anggota menunggu bukti

## Tujuan
Menetapkan kontrak perangkat lunak, kepemilikan modul, dan cara integrasi sebelum pengembangan berjalan paralel. Setiap anggota harus dapat memakai modul lain melalui antarmuka yang sama tanpa mengubah konfigurasi CubeMX atau API secara sepihak.

## Sub-milestone
- [x] M0.1 Kontrak fungsi dan tipe data ditetapkan di `config.h`, `params.h`, `sensor.h`, dan `motor.h`
- [x] M0.2 Struktur repo, aturan branch (`feat/sensor`, `feat/motor`, `feat/ui`), dan format commit ditetapkan
- [x] M0.3 Pembagian kepemilikan A/B/C dan jadwal sinkron tiap 2–3 hari ditetapkan
- [ ] M0.4 Setiap anggota membuktikan dapat clone dan build baseline di STM32CubeIDE

## Keputusan yang berlaku

### Kontrak perangkat lunak

| Modul | API / tipe | Kontrak |
|---|---|---|
| Sensor | `Sensor_ReadRaw(uint16_t raw[16])` | Menghasilkan ADC mentah 16 kanal. Indeks `group * 4 + mux_state`; urutan fisik diverifikasi pada M02. |
| Sensor | `Sensor_GetPosition()` | Posisi garis `-7500..+7500`; `0` berarti garis di tengah. |
| Sensor | `Sensor_LineVisible()` | `false` bila garis tidak terlihat. |
| Motor | `Motor_Set(int16_t left, int16_t right)` | Perintah tiap motor dibatasi ke `-1000..+1000`. Nilai positif adalah maju setelah arah fisik dikonfirmasi di M03. |
| Motor | `Motor_Brake()`, `Motor_Coast()` | Rem aktif: kedua pin HIGH. Coast: kedua pin LOW. |
| Parameter | `Params_t` | Memuat PID, batas kecepatan, kalibrasi sensor, flag `invert`, magic, dan checksum. UI mengubahnya; storage menyimpan; PID/track membacanya. |
| Aplikasi | `App_Init()`, `App_Update()` | Titik integrasi aplikasi. Dipanggil dari blok `USER CODE` di `main.c`. |

`config.h` adalah sumber kebenaran untuk konstanta bersama. Perubahan pada API atau struktur `Params_t` harus dibahas pada sinkronisasi dan dicatat di `Documentation/PROJECT.md` sebelum di-merge.

### Kepemilikan modul

| Peran | Pemilik | Ruang lingkup | Branch |
|---|---|---|---|
| A — Sensing & Kontrol | Tentukan nama anggota A | sensor, calibration, pid, `Tools/uart_plot.py` | `feat/sensor` |
| B — Motor & Hardware | Tentukan nama anggota B | motor, pengukuran, mekanik, lintasan uji | `feat/motor` |
| C — UI & Sistem | Tentukan nama anggota C | ui, buttons, storage, track, app, `params.h` | `feat/ui` |
| Bersama | Semua | `config.h`, `.ioc`, README, keputusan antarmuka | sesuai kebutuhan |

Satu modul memiliki satu pemilik. Perubahan pada modul milik orang lain diajukan lewat issue atau diskusi, lalu dikerjakan pemiliknya atau disetujui secara eksplisit.

### Aturan Git dan integrasi

- Branch fitur dibuat dari `main` dan memakai nama yang telah ditetapkan. Integrasi ke `main` dilakukan setelah target milestone dan build lokal lolos.
- Format commit: `[M<n>] <modul>: <perubahan singkat>`, contoh `[M2] sensor: add mux scan`.
- Satu commit berisi satu perubahan fungsional yang dapat dibuild; jangan mencampur refactor tidak terkait dengan perubahan perilaku.
- File `.ioc` hanya diubah oleh satu orang pada satu waktu. Pemilik perubahan wajib memberi tahu tim sebelum mengubahnya dan melakukan regenerate terlebih dahulu.
- Pada berkas yang dihasilkan CubeMX, kode manual hanya diletakkan di blok `USER CODE BEGIN/END`.
- Tidak ada force-push atau perubahan langsung ke `main` tanpa persetujuan anggota lain.

### Ritme sinkronisasi

Sinkronisasi dilakukan setiap 2–3 hari, dimulai pada 6 Nov dan berlanjut pada 9, 12, 15, 18, 21, 24, dan 26 Nov. Agenda maksimum 15 menit: status tiap modul, blocker, keputusan antarmuka/pin, hasil uji, dan pekerjaan sebelum pertemuan berikutnya. Keputusan ditulis di `Documentation/PROJECT.md` atau milestone terkait pada hari yang sama.

## Selesai kalau

- Nama anggota A, B, dan C tercantum pada tabel kepemilikan.
- Semua anggota bisa clone repository dan build baseline `Firmware/LF_TED2026` di STM32CubeIDE tanpa error.
- Firmware baseline dapat di-flash melalui ST-Link dan UART mengeluarkan teks uji pada M01.
- Kontrak API di atas disetujui tim dan tidak ada perubahan antarmuka terbuka tanpa catatan.

## Langkah yang dikerjakan

1. Gunakan `Documentation/PROJECT.md` sebagai ringkasan kontrak lintas-modul dan header firmware sebagai definisi API.
2. Pastikan target MCU adalah STM32F411CEU6 dan project dibuat/dibuka dari `Firmware/LF_TED2026`.
3. Sebelum mengerjakan modul, buat branch sesuai kepemilikan lalu konfirmasi baseline project dapat dibuild.
4. Simpan konfigurasi pin, library, dan perubahan CubeMX di M01; jangan menetapkan pin pengganti PA11, frekuensi PWM, atau periode loop sebagai keputusan final sebelum pengujian milestone pemiliknya.
5. Catat hasil clone-build setiap anggota di tabel hasil uji berikut.

## Hasil uji
| Uji | Hasil | Catatan |
|---|---|---|
| Kontrak API vs header firmware | Lulus | `sensor.h`, `motor.h`, `params.h`, `config.h`, dan `app.h` sesuai kontrak. |
| Struktur repo dan aturan Git | Lulus | Struktur serta aturan branch/commit telah didokumentasikan di README. |
| Clone dan build anggota A | Menunggu | Isi nama anggota, OS/CubeIDE, commit, dan hasil build. |
| Clone dan build anggota B | Menunggu | Isi nama anggota, OS/CubeIDE, commit, dan hasil build. |
| Clone dan build anggota C | Menunggu | Isi nama anggota, OS/CubeIDE, commit, dan hasil build. |

## Masalah & solusi

- Nama anggota belum dicatat di repository. Isi tabel kepemilikan sebelum pekerjaan paralel dimulai.
- PA11 berkonflik dengan USB D-. Pilihan pin pengganti ditetapkan dan diuji pada M01.3.
- Frekuensi PWM, periode loop, dan nilai PID saat ini adalah titik awal, bukan parameter final. Masing-masing diputuskan setelah pengukuran pada M03 dan M08.

## Catatan untuk milestone berikutnya

- M01 mengunci konfigurasi CubeMX, pin pengganti PA11, SWD, dan UART debug.
- M02 memverifikasi urutan fisik sensor dan waktu scan penuh.
- M03 mengonfirmasi arah motor serta frekuensi PWM yang layak.
- M08 mengukur dan menetapkan periode loop kontrol serta parameter PID hasil tuning.
