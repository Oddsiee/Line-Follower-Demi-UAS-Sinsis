# LF_TED2026 (Firmware)

Lihat `SETUP_CUBEIDE.md` untuk konfigurasi `.ioc` dan cara menghubungkan `Core/Inc` + `Core/Src` ke `main.c`.

**Mau test OLED saja?** Cara tercepat — tanpa CubeIDE:
```powershell
cd Tests\test_oled
.\build.ps1
.\flash.ps1
```
Lihat `Tests/test_oled/README.md`. Panduan lewat CubeIDE ada di `TEST_OLED.md`.

Kode modul masih **stub** (ada `TODO` dengan nomor milestone). Kontrak antarmuka ada di `Documentation/PROJECT.md`.
