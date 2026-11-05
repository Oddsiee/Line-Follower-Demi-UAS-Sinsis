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
Folder `test_sensor/` dan `test_motor/` dibuat oleh A dan B saat Milestone 2 dan 3.
