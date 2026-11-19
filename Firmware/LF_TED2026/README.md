# LF_TED2026 (Arsip Firmware Awal — Tidak Dipakai untuk Coding Utama)

> **PEMBERITAHUAN PENTING (ATURAN LOKASI CODING UTAMA):**
> Folder `Firmware/LF_TED2026/` ini **sudah tidak digunakan untuk penulisan coding utama**.
> - Codingan utama robot Line Follower **HANYA** ditulis pada folder **`Line Follower Coding Utama/`** karena `.ioc` utama (**`Line Follower Coding Utama/Line Follower Coding Utama.ioc`**, MCU **`STM32F401CCU6`**) berada di folder tersebut.
> - **TIDAK ADA coding untuk robot Line Follower di luar folder `Line Follower Coding Utama/`**.

Lihat `SETUP_CUBEIDE.md` untuk referensi konfigurasi pin `.ioc` dan cara menghubungkan modul ke `main.c`.

**Mau test OLED saja?** Cara tercepat — tanpa CubeIDE:
```powershell
cd Tests\test_oled
.\build.ps1
.\flash.ps1
```
Lihat `Tests/test_oled/README.md`. Panduan lewat CubeIDE ada di `TEST_OLED.md`.

Kode modul masih **stub** (ada `TODO` dengan nomor milestone). Kontrak antarmuka ada di `Documentation/PROJECT.md`.
