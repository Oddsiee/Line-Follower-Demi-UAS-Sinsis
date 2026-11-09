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

Folder `test_motor/` dibuat oleh B saat Milestone 3.
