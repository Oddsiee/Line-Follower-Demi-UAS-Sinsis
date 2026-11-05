# Diagnosa OLED Gelap — Hasil Pengukuran Hardware

Tanggal: 2026-11-05
Board: STM32F411CEU6 (Black Pill), ST-Link V2J48S7

## ✅ STATUS AKHIR: SELESAI — OLED MENYALA

**Akar masalah:** `MX_I2C1_Init()` di `main.c` tidak pernah memanggil
`__HAL_RCC_I2C1_CLK_ENABLE()`. Tanpa clock, register I2C1 tidak bisa diakses
sama sekali — peripheral-nya mati walau kodenya terlihat benar.

**Perbaikan:**
1. Tambah `__HAL_RCC_I2C1_CLK_ENABLE()` di `MX_I2C1_Init()` (`Tests/test_oled/main.c`).
2. Tambah `i2c_bus_recover()` di `SH1106_Init()` (`Firmware/LF_TED2026/Core/Src/sh1106.c`)
   — clock SCL 9× + reset peripheral, supaya flag BUSY tidak macet permanen
   kalau MCU reset di tengah transaksi I2C.

**Verifikasi:** `g_sh1106_ok` naik terus, `g_sh1106_fail` = 0.

---

## Ringkasan awal (sebelum diperbaiki)

**OLED tidak terdeteksi di bus I2C sama sekali.** Bukan masalah driver, bukan
masalah firmware, bukan masalah alamat I2C. Masalahnya di **level fisik/elektrikal**.

## Data pengukuran (dibaca langsung dari register MCU via SWD)

### 1. Scan I2C — semua alamat 0x08..0x77

| Bus | Pin | Hasil |
|---|---|---|
| bit-bang software | PB6/PB7 | **0 device** |
| I2C1 (AF4) | PB6/PB7 | **0 device** |
| I2C1 (AF4) | PB8/PB9 | **0 device** |
| I2C2 (AF4) | PB10/PB11 | **0 device** |
| I2C3 (AF4) | PA8/PC9 | **0 device** |

Tidak ada satu pun device I2C yang ACK di bus mana pun.

### 2. Uji kondisi pin (PB6=SCL, PB7=SDA)

| Kondisi pin | Hasil | Arti |
|---|---|---|
| Input + pull-up internal | `0xC0` → SCL=1, SDA=1 | Pin MCU sehat, tidak short ke GND |
| Input + pull-down internal | `0x00` → SCL=0, SDA=0 | **Tidak ada pull-up eksternal** |
| Input float (tanpa pull) | `0x00` → SCL=0, SDA=0 | Tidak ada yang menarik ke HIGH |
| Output push-pull HIGH | `0xC0` → SCL=1, SDA=1 | MCU bisa drive HIGH |
| Output push-pull LOW | `0x00` → SCL=0, SDA=0 | MCU bisa drive LOW |

### 3. Uji pembeda (pull-down vs pull-up)

```
g_pu_both = 0xC0   (pull-up internal  -> kedua line HIGH)
g_pd_both = 0x00   (pull-down internal -> kedua line LOW)
g_float   = 0x00   (float -> kedua line LOW)
```

**Ini tes penentu.** Modul OLED SH1106 normal punya resistor pull-up
4.7k–10kΩ di jalur SCL dan SDA. Pull-up 10k itu akan **mengalahkan**
pull-down internal MCU (~40kΩ), sehingga line tetap terbaca HIGH.

Karena dengan pull-down line terbaca **LOW**, berarti:

> **Tidak ada pull-up di bus I2C sama sekali.**

## Kesimpulan

Pin MCU sehat (bisa drive HIGH dan LOW, tidak short). Tapi bus I2C tidak punya
pull-up, dan tidak ada device yang ACK. Kemungkinan penyebab, urut prioritas:

1. **Modul OLED tidak mendapat daya** — VCC/GND tertukar, atau VCC tidak
   tersambung ke 3V3.
2. **Urutan pin modul berbeda** — dokumentasi menyebut `VCC-GND-SCL-SDA`,
   tapi banyak modul 1.3" justru `GND-VCC-SCL-SDA`. Kalau tertukar,
   modul tidak berdaya dan gejalanya persis seperti ini.
3. **Kabel SCL/SDA tidak benar-benar kontak** ke PB6/PB7.
4. **Modul OLED rusak.**

## Yang harus dilakukan

### Langkah 1 — Ukur dengan multimeter (paling cepat)

Ukur tegangan di pin modul OLED, dengan board menyala:

| Pin modul | Harus terbaca |
|---|---|
| VCC | **3.2–3.4 V** |
| GND | 0 V |
| SCL | 3.3 V (idle) |
| SDA | 3.3 V (idle) |

- Kalau **VCC = 0 V** → modul tidak dapat daya. Cek urutan pin / kabel.
- Kalau **SCL atau SDA = 0 V** → tidak ada pull-up, atau kabel putus.

### Langkah 2 — Cek urutan pin modul

Lihat label di PCB modul. Urutan yang umum:

```
Modul A: VCC  GND  SCL  SDA     <- sesuai dokumentasi proyek
Modul B: GND  VCC  SCL  SDA     <- sering dijual, URUTAN TERBALIK
```

Kalau modul kamu tipe B, maka VCC dan GND tertukar → modul mati.

### Langkah 3 — Tambah pull-up eksternal (kalau modul tidak punya)

Solder resistor **4.7kΩ** dari **SCL ke 3V3** dan **4.7kΩ** dari **SDA ke 3V3**.
Banyak modul OLED sudah punya, tapi kalau tidak ada, I2C tidak akan jalan.

### Langkah 4 — Ulangi tes

Setelah wiring diperbaiki:

```powershell
cd Tests\test_oled
.\run_pullup.ps1     # harus: both pull-down = 0xC0 (HIGH)
.\read_multibus.ps1  # harus: bus B (PB6/PB7) menemukan device di 0x3C
.\build.ps1
.\flash.ps1          # OLED menyala
```

## Catatan teknis

- Semua pengukuran dilakukan dengan membaca register `GPIOB->IDR` langsung
  via SWD, bukan menebak dari kode.
- `STM32_Programmer_CLI -r32 <addr> <n>` — parameter kedua adalah **jumlah BYTE**,
  bukan jumlah word. Ini sempat menyebabkan hasil salah baca.
- Setiap `-c port=SWD` baru akan **reset MCU**, jadi semua variabel harus
  dibaca dalam satu koneksi. Pakai `mode=hotplug` untuk membaca tanpa reset.
- **Pelajaran utama:** kalau I2C tidak jalan, cek `__HAL_RCC_I2Cx_CLK_ENABLE()`
  dulu sebelum curiga wiring. Peripheral tanpa clock = register tidak bisa dibaca,
  gejalanya persis seperti "device tidak ada".

## Alat diagnosa yang tersedia

| Script | Fungsi |
|---|---|
| `run_pullup.ps1` | Cek ada/tidak pull-up di bus (pull-down vs pull-up) |
| `run_slowscan.ps1` | Scan I2C bit-bang ~5 kHz (tahan pull-up lemah) |
| `run_speedtest.ps1` | Cari kecepatan I2C yang bekerja + layar putih |
| `read_multibus.ps1` | Scan 5 bus I2C berbeda |
| `read_pintest.ps1` | Uji pin bisa drive HIGH/LOW |
| `read_all.ps1` | Baca semua variabel `g_*` dalam satu snapshot SWD |
