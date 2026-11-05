# Test OLED SH1106 (STM32CubeIDE + HAL)

File yang terlibat:

| File | Fungsi |
|---|---|
| `Core/Inc/sh1106.h` / `Core/Src/sh1106.c` | Driver SH1106 mandiri (init, framebuffer, drawing, font 6x8). Tidak butuh library eksternal. |
| `Core/Inc/test_oled.h` / `Core/Src/test_oled.c` | Rutin test: scan I2C + 4 layar demo otomatis. |
| `Tools/gen_font.py` | (opsional) generator font 6x8, kalau mau ganti font. |

## 0. Bikin project CubeIDE dulu (WAJIB — repo ini belum punya project siap build)

Folder `Firmware/LF_TED2026/` **hanya** berisi `.ioc` + `Core/Inc` + `Core/Src`.
Belum ada `.cproject`, `Drivers/`, `Startup/`, dan `main.c`. Jadi:

1. Buka **STM32CubeIDE** → pilih workspace (mis. `C:\Users\<kamu>\STM32CubeIDE\workspace_2.2.0`).
2. **File → New → STM32 Project**.
3. Tab **MCU/MPU Selector** → cari `STM32F411CEUx` → pilih **STM32F411CEU6** → **Next**.
4. Project Name: **`Tes_Oled_Sensor_Baru`** (harus sama dengan `ProjectManager.ProjectName` di `.ioc`, kalau beda CubeMX akan rename sendiri).
5. **Finish** → muncul dialog "Initialize all peripherals with their default mode?" → pilih **No**.
6. Salin `Firmware/LF_TED2026/Tes_Oled_Sensor_Baru.ioc` dari repo ke folder project yang baru dibuat (timpa `.ioc` bawaan).
7. Di Project Explorer, klik kanan `.ioc` → **Open With → Device Configuration Tool**.
8. Cek **Connectivity → I2C1** sudah `I2C`, pin **PB6 = I2C1_SCL**, **PB7 = I2C1_SDA**, ClockSpeed 400 kHz. (File `.ioc` repo sudah begini, jadi biasanya tinggal lihat saja.)
9. **Ctrl+S** (atau **Project → Generate Code**).

> Kalau langkah 6–7 ribet, alternatifnya: bikin project baru kosong, lalu di CubeMX centang I2C1 manual + set PB6/PB7. Hasilnya sama.

### Kalau muncul error "Firmware Package ... not found"

Gejala: saat bikin project / Generate Code, CubeIDE bilang firmware package `STM32Cube FW_F4 V1.28.3` tidak ditemukan, dan tombol **Firmware Updater** gagal mengunduh (timeout ke `www.st.com`).

Penyebab: folder `C:\Users\<kamu>\STM32Cube\Repository` kosong, dan jaringan tidak bisa akses `www.st.com`.

**Solusi yang sudah diterapkan di mesin ini:** package `STM32Cube_FW_F4_V1.28.3` dirakit manual dari repo GitHub ST (HAL driver + CMSIS device + CMSIS core) dan dipasang ke:

```
C:\Users\<kamu>\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3\
├── package.xml
├── Drivers\
│   ├── STM32F4xx_HAL_Driver\  (Inc + Src, 94 file .c)
│   └── CMSIS\
│       ├── Device\ST\STM32F4xx\  (Include + Source/Templates/gcc/startup_stm32f411xe.s)
│       └── Include\              (core_cm4.h, cmsis_gcc.h, dll)
└── Projects\
```

Sudah diverifikasi: `sh1106.c` + `test_oled.c` + HAL I2C **compile & link bersih** jadi `.elf` (text 72 KB) memakai package ini.

Kalau perlu ulang di komputer lain, langkahnya:
```powershell
$base = "$env:TEMP\fwf4"; New-Item -ItemType Directory $base -Force | Out-Null
Push-Location $base
git clone --depth 1 https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git hal
git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_f4.git dev
git clone --depth 1 https://github.com/ARM-software/CMSIS_5.git cmsis5
Pop-Location

$dst = "$env:USERPROFILE\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3"
New-Item -ItemType Directory "$dst\Drivers\STM32F4xx_HAL_Driver" -Force | Out-Null
Copy-Item "$base\hal\Inc","$base\hal\Src" "$dst\Drivers\STM32F4xx_HAL_Driver\" -Recurse -Force
New-Item -ItemType Directory "$dst\Drivers\CMSIS\Device\ST\STM32F4xx" -Force | Out-Null
Copy-Item "$base\dev\Include","$base\dev\Source" "$dst\Drivers\CMSIS\Device\ST\STM32F4xx\" -Recurse -Force
New-Item -ItemType Directory "$dst\Drivers\CMSIS\Include" -Force | Out-Null
Copy-Item "$base\cmsis5\CMSIS\Core\Include\*" "$dst\Drivers\CMSIS\Include\" -Force
```

Lalu buat `$dst\package.xml`:
```xml
<?xml version="1.0" encoding="ISO-8859-1" standalone="no"?>
<Package DBVersion="2.0">
 <PackDescription Release="FW.F4.1.28.0" Patch="FW.F4.1.28.3">
  <Note Release="ReleaseNotes.html" Patch="ReleaseNotes_Patch.html"/>
 </PackDescription>
</Package>
```

Restart CubeIDE, lalu coba lagi.

## 1. Konfigurasi `.ioc` (CubeMX)

Di project CubeIDE kamu (MCU STM32F411CEUx):

1. **Connectivity → I2C1 → pilih "I2C"** (bukan disable).
2. Pastikan pin otomatis terpasang: **PB6 = I2C1_SCL**, **PB7 = I2C1_SDA**.
3. **Configuration → I2C1 → Parameter Settings**:
   - I2C Speed Mode = **Fast Mode** (400 kHz). (100 kHz juga jalan, cuma refresh lebih lambat.)
   - Sisanya biarkan default.
4. **Project Manager** (pastikan): "Generate peripheral initialization as a pair of .c/.h files" **tidak** perlu dicentang — `hi2c1` otomatis jadi global di `main.c` (yang dipakai driver).
5. **Generate Code**.

> Kalau ada error compile `I2C_HandleTypeDef hi2c1` tidak dikenal, pastikan `MX_I2C1_Init()` dipanggil di `main()` dan `hi2c1` dideklarasikan global.

## 2. Masukkan file ke project

Salin 4 file ini dari repo ke project CubeIDE (folder `Core/Inc` dan `Core/Src`):

| Dari repo | Ke project |
|---|---|
| `Firmware/LF_TED2026/Core/Inc/sh1106.h` | `Core/Inc/sh1106.h` |
| `Firmware/LF_TED2026/Core/Inc/test_oled.h` | `Core/Inc/test_oled.h` |
| `Firmware/LF_TED2026/Core/Src/sh1106.c` | `Core/Src/sh1106.c` |
| `Firmware/LF_TED2026/Core/Src/test_oled.c` | `Core/Src/test_oled.c` |

**Jangan** salin `app.c`, `sensor.c`, `motor.c`, `ui.c`, dll. — itu masih stub dan butuh modul lain. Untuk test OLED cukup 4 file di atas.

Setelah disalin, di Project Explorer klik kanan project → **Refresh (F5)** supaya file baru terdeteksi.

> Kalau muncul error `undefined reference to hi2c1`, berarti `MX_I2C1_Init()` belum dipanggil di `main()`. Pastikan I2C1 aktif di `.ioc` lalu Generate Code ulang.

## 3. Hubungkan di `main.c`

```c
/* USER CODE BEGIN Includes */
#include "test_oled.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
TestOled_Init();
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
  /* USER CODE BEGIN 3 */
  TestOled_Run();
  /* USER CODE END 3 */
}
/* USER CODE END WHILE */
```

## 4. Build & flash

### 4a. Build
**Project → Build Project** (ikon palu, atau `Ctrl+B`).

Hasil: `Debug/Tes_Oled_Sensor_Baru.elf` di folder project.
Kalau ada error, baca tab **Console** — biasanya `hi2c1` tidak dikenal (I2C1 belum aktif) atau file `.c` belum masuk build.

### 4b. Sambungkan ST-Link
Wiring ST-Link V2 (clone biru) ke Black Pill:

| ST-Link | Black Pill |
|---|---|
| `3.3V` | `3V3` |
| `GND` | `GND` |
| `SWDIO` | `PA13` (pin DIO) |
| `SWCLK` | `PA14` (pin CLK) |

> **Jangan** sambungkan `5V` ST-Link ke board. Cukup 3.3V.
> OLED tetap tersambung: `VCC`→3V3, `GND`→GND, `SCL`→PB6, `SDA`→PB7.

Cek ST-Link kebaca:
```powershell
& "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -l
```
Harus muncul `ST-Link Probe ... STM32F411xx`. Kalau `No ST-Link detected`:
- Cek kabel USB (harus kabel data, bukan charge-only).
- Pasang driver ST-Link: `C:\Program Files (x86)\STMicroelectronics\stlink_server` atau lewat **STM32CubeProgrammer → Help → Install Drivers**.
- Coba port USB lain (hindari hub).

### 4c. Flash — cara 1: dari CubeIDE (paling gampang)
Klik kanan project → **Run As → STM32 C/C++ Application** (atau ikon ▶ hijau).
CubeIDE otomatis build + flash + langsung jalan. Kalau diminta konfigurasi debug, pilih **ST-LINK (OpenOCD)** → **OK**.

### 4d. Flash — cara 2: STM32CubeProgrammer GUI
1. Buka **STM32CubeProgrammer**.
2. Kiri atas: pilih **ST-LINK**, klik **Connect**.
3. Tab **Erasing & Programming** → **Browse** → pilih `Debug/Tes_Oled_Sensor_Baru.elf`.
4. Centang **Verify programming** → **Start Programming**.
5. **Disconnect**, tekan tombol **RESET** di board.

### 4e. Flash — cara 3: CLI (kalau GUI males)
```powershell
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
& $cli -c port=SWD -w "C:\path\ke\project\Debug\Tes_Oled_Sensor_Baru.elf" -v -rst
```
`-v` = verify, `-rst` = reset setelah flash.

## Hasil yang diharapkan

Layar berganti otomatis tiap 2 detik:

1. **Boot** — judul + info chip.
2. **I2C scan** — `OLED @ 0x3C OK` (atau `0x3D`, atau `TIDAK TERDETEKSI`).
3. **Shapes** — lingkaran, kotak, garis.
4. **Live simulasi** — 16 batang "sensor" + marker garis yang jalan kiri-kanan.

## Troubleshooting

| Gejala | Solusi |
|---|---|
| Layar gelap total | Cek hasil scan (layar 2). Kalau `TIDAK TERDETEKSI`: cek kabel SCL/SDA/VCC/GND dan pastikan `MX_I2C1_Init()` jalan. |
| Terdeteksi di `0x3D` | Ubah `SH1106_I2C_ADDR` jadi `0x3D` di `sh1106.h`. |
| Gambar terbalik / mirror | Tukar `0xA1`↔`0xA0` dan `0xC8`↔`0xC0` di `sh1106.c` (fungsi `SH1106_Init`). |
| Baris terpotong / acak | Pastikan PB6/PB7 tidak dipakai peripheral lain (cocok dengan pinout master). |
| Kontras terlalu terang/gelap | Ubah nilai `0xCF` (contrast) di `SH1106_Init`. |
| `No ST-Link detected` | Cek kabel USB data, pasang driver ST-Link, ganti port USB. Coba `STM32_Programmer_CLI.exe -l`. |
| Flash sukses tapi layar tetap gelap | Pastikan `TestOled_Init()` benar-benar dipanggil di `USER CODE BEGIN 2` (bukan di luar blok USER CODE — akan terhapus saat Generate Code). |
| Build error `hi2c1 undeclared` | I2C1 belum aktif di `.ioc`, atau `MX_I2C1_Init()` tidak dipanggil. Generate Code ulang. |
| `Firmware Package STM32Cube FW_F4 V1.28.3 not found` | Package firmware belum terpasang. Lihat bagian **0** di atas — sudah dirakit manual di `C:\Users\<kamu>\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3`. |
| Firmware Updater gagal / timeout | `www.st.com` tidak bisa diakses dari jaringan ini. Pakai cara manual di bagian **0**. |

## Catatan

- File `Tests/Test OLED/Build/Tes_OLED_Sensor.elf` yang ada di repo adalah **build lama** (tidak berisi kode test OLED versi sekarang). Jangan di-flash; build sendiri dari source.
- Driver ini **tanpa library eksternal** (tidak perlu U8g2). Nanti untuk `ui.c`, `sh1106.c/h` bisa langsung dipakai ulang sebagai lapisan gambar.
- Kalau mau font berbeda/lebih besar, edit `Tools/gen_font.py` lalu jalankan (butuh Python + Pillow), salin hasilnya ke tabel `FONT6x8` di `sh1106.c`.
