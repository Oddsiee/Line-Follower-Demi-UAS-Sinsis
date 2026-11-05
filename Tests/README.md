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

## test_sensor/ — SIAP PAKAI

Test sensor 16CH Dariyan-X standalone (ADC1 PA4–PA7 + mux PB0/PB1):
```powershell
cd Tests\test_sensor
.\build.ps1
.\flash.ps1
.\read_sensor.ps1 -Samples 10 -IntervalMs 500
```
Lihat `test_sensor/README.md` untuk tabel wiring pin sensor dan troubleshooting.

Folder `test_motor/` dibuat oleh B saat Milestone 3.
