# Hardware

> **ATURAN MUTLAK LOKASI CODING UTAMA:** Codingan utama robot Line Follower **HANYA** ditulis pada folder **`Line Follower Coding Utama/`** (lokasi `.ioc` utama `Line Follower Coding Utama/Line Follower Coding Utama.ioc`, target MCU `STM32F401CCU6`). **TIDAK ADA** coding untuk robot Line Follower di luar folder **`Line Follower Coding Utama/`** (implementasi modul berada di `Line Follower Coding Utama/Core/Src/` dan `Line Follower Coding Utama/Core/Inc/`).

Acuan wiring terbaru ada di [dokumentasi master](../../dokumentasi_master_robot_line_follower_ted_2026.md): sensor PA0=Out1, PA1=Out2, PA2=Out3, PA3=In1, PB0=In2, PB1=Out4; tombol PA7/PA6/PA5/PA4/R, OLED PB7=SDA dan PB6=SCL, driver motor PA8/PA9/PA10/PA11.

Pindahkan ke folder ini:
- `dokumentasi_master_robot_line_follower_ted_2026.md`
- `Guidebook LF TED 2O26.pdf`

Gunakan `git mv` agar history tetap terjaga:
```
git mv dokumentasi_master_robot_line_follower_ted_2026.md Documentation/Hardware/
git mv "Guidebook LF TED 2O26.pdf" Documentation/Hardware/
```
