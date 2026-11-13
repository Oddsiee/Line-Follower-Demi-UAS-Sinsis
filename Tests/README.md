# Tests

Firmware/sketsa uji per modul.

## test_oled/ — SIAP PAKAI

Test OLED SH1106 standalone, **tanpa STM32CubeIDE**:
```powershell
cd Tests\test_oled
.\build.ps1
.\flash.ps1
```
Lihat `test_oled/README.md` untuk detail.

## Folder lain

Folder lama `Test OLED` (berisi build lama) bisa dihapus:
```
git rm -r "Tests/Test OLED"
```

## test_sensor/ — PERLU PENYESUAIAN PIN

Test sensor 16CH Dariyan-X standalone (wiring terbaru: Out1–Out4 pada PA0/PA1/PA2/PB1, ADC1_IN0/IN1/IN2/IN9; mux In1=PA3 dan In2=PB0).
Sesuaikan GPIO analog, kanal/rank ADC, serta GPIO selektor mux pada firmware uji dan diagnostik sebelum menjalankan perintah berikut; lihat `test_sensor/README.md`.

```powershell
cd Tests\test_sensor
.\build.ps1
.\flash.ps1
.\read_sensor.ps1 -Samples 10 -IntervalMs 500
```
Lihat `test_sensor/README.md` untuk tabel wiring pin sensor dan troubleshooting.

## test_motor/ — SIAP PAKAI

Test driver motor ZK-5AD (TIM1 PA8–PA11 = D0–D3), standalone tanpa STM32CubeIDE.
Menguji koneksi STM32 ↔ driver + arah putar tiap roda (fase auto: pin walk D0–D3,
maju/kiri/kanan/mundur/rem).

```powershell
cd Tests\test_motor
.\build.ps1
.\flash.ps1
.\read_motor.ps1 -Samples 5 -IntervalMs 500   # baca g_* + register TIM1 via SWD
```

**Angkat/ganjal roda sebelum flash** — motor langsung berputar.
Lihat `test_motor/README.md` untuk wiring, tabel fase, dan troubleshooting.

## test_overall/ — SIAP PAKAI

Test terpadu interaktif (Tombol + OLED + Sensor 16CH + Motor ZK-5AD + Monitor 3.3V `Vdda`) untuk pengujian mandiri pakai baterai/buck converter tanpa ST-Link:
```powershell
cd Tests\test_overall
.\build.ps1
.\flash.ps1
```
Lihat `test_overall/README.md` untuk panduan menu dan kontrol tombol.
