# PROJECT — Kontrak Antarmuka & Aturan Tim

## Kontrak antarmuka (jangan diubah sepihak)
| Modul | Fungsi | Keterangan |
|---|---|---|
| sensor | `Sensor_ReadRaw(uint16_t raw[16])` | Nilai ADC mentah 16 channel |
| sensor | `Sensor_GetPosition()` | -7500..+7500, 0 = garis di tengah |
| sensor | `Sensor_LineVisible()` | false jika garis hilang |
| motor | `Motor_Set(int16_t kiri, int16_t kanan)` | -1000..+1000 |
| motor | `Motor_Brake()`, `Motor_Coast()` | Rem aktif / bebas |
| params | `Params_t` (params.h) | Kp, Ki, Kd, base/max speed, kalibrasi, flag invert |

Selama kontrak ini tetap, tiap orang boleh memakai stub modul lain.

## Alur data
```
Sensor_ReadRaw -> Calibration (normalisasi) -> Sensor_GetPosition
  -> PID -> Motor_Set
  Track (persimpangan/CP/invert) memodifikasi base speed & flag invert
  UI menampilkan data dan mengedit Params_t; Storage menyimpan ke Flash
```

## Pembagian modul
| Peran | File |
|---|---|
| A | sensor, calibration, pid, Tools/uart_plot.py |
| B | motor, Tests/test_motor, dokumen hardware & pengukuran |
| C | ui, buttons, storage, track, app, params.h |
| Bersama | config.h, .ioc, README.md |

## Hal yang wajib diingat (dari dokumentasi master)
- OLED memakai **SH1106**, bukan SSD1306 (pakai U8g2).
- Pin `3V3` dan `VT` di header ZK-5AD **tidak disambung**.
- Rem aktif = kedua pin driver HIGH; coast = kedua LOW.
- Batas regulasi: 20x20x10 cm, 3 kg, baterai 12 V +-1,5 V.
- PA11 (PWM motor kanan IN2) juga pin USB D-; keputusan pin pengganti dicatat di Milestone 1.

## Keputusan terbuka
- [ ] Pin pengganti PA11 (M1.3)
- [ ] Frekuensi PWM motor (M3.1)
- [ ] Periode loop kontrol (M8.1)
