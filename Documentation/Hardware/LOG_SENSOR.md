# Log Pengukuran Sensor 16CH Dariyan-X

Catatan hasil pengukuran nyata dari hardware. Dipakai sebagai acuan
kalibrasi dan debugging ke depan.

**Setup uji:** STM32F411CEU6 (Black Pill), sensor diberi daya dari
power supply eksternal, GND power supply disambung ke GND STM32 + ST-Link.

---

## 2026-11-06 — Baseline kertas PUTIH

**Kondisi:** semua 16 sensor di atas kertas putih, ground bersama,
firmware `Tests/test_sensor` (oversampling 4x, settling 50 us).

| Kanal | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Nilai | 240 | 243 | 231 | 229 | 222 | 226 | 226 | 219 |

| Kanal | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|
| Nilai | 245 | 251 | 196 | 189 | 228 | 232 | 202 | 196 |

**Statistik:**
- Rentang: **189 – 251**
- Rata-rata: **~222**
- Variasi antar sampel: **±10–20**
- Kanal saturasi (≥4090): **0/16**
- Waktu scan penuh: **9.1 ms** (4 mux × 4 kanal × oversample 4)

**Catatan:** nilai putih relatif RENDAH. Kemungkinan modul ini output-nya
*inverted* (cahaya pantul banyak → tegangan output turun). Perlu dikonfirmasi
dengan pengukuran kertas hitam.

---

## 2026-11-06 — Karakterisasi mux (sebelum ground diperbaiki)

Data ini **TIDAK VALID** untuk kalibrasi, tapi berguna sebagai pembanding
gejala. Diambil saat GND power supply belum disambung ke GND STM32.

| idx (g*4+s) | mean | min | max | spread |
|---|---|---|---|---|
| 0 | 349 | 0 | 916 | 916 |
| 1 | 332 | 0 | 930 | 930 |
| 2 | 2077 | 0 | 4095 | 4095 |
| 3 | 2223 | 0 | 4095 | 4095 |
| 4 | 391 | 0 | 953 | 953 |
| 5 | 326 | 0 | 912 | 912 |
| 6 | 2086 | 0 | 4095 | 4095 |
| 7 | 2146 | 0 | 4095 | 4095 |
| 8 | 370 | 0 | 951 | 951 |
| 9 | 324 | 0 | 949 | 949 |
| 10 | 2105 | 0 | 4095 | 4095 |
| 11 | 2134 | 0 | 4095 | 4095 |
| 12 | 314 | 0 | 902 | 902 |
| 13 | 370 | 0 | 913 | 913 |
| 14 | 2093 | 0 | 4095 | 4095 |
| 15 | 2175 | 0 | 4095 | 4095 |

**Gejala khas ground tidak bersama:**
- Spread ~900–950 di SEMUA kanal
- State mux 2 dan 3 (In2=1) selalu saturasi 4095
- Nilai acak 0..950, mean ~350
- Terjadi BAHKAN saat mux ditahan di satu state

---

## 2026-11-06 — VREFINT (referensi internal MCU)

Dipakai untuk memisahkan masalah ADC vs masalah wiring sensor.

| Kondisi | mean | min | max | spread | Kesimpulan |
|---|---|---|---|---|---|
| GND terpisah | 1508 | 1506 | 1511 | 5 | ADC sehat, masalah di wiring |
| GND bersama | 1509 | 1507 | 1529 | 22 | ADC sehat |

**Nilai normal VREFINT: ~1500 @ Vdda 3.3 V.**
Spread <50 masih normal. Kalau spread besar → cek suplai Vdda MCU.

---

## Cara reproduksi

```powershell
cd Tests\test_sensor

# Firmware normal (scan terus-menerus)
.\build.ps1
.\flash.ps1
.\read_sensor.ps1 -Samples 3 -IntervalMs 900

# Firmware diagnostik (VREFINT + polling vs DMA + karakterisasi mux)
.\build_diag.ps1
.\read_diag.ps1
```

---

## Checklist wiring (WAJIB)

| Pin sensor | Nama | → STM32 |
|---|---|---|
| 1 | Out1 | PA4 |
| 2 | Gnd | GND |
| 3 | Out2 | PA5 |
| 4 | Out3 | PA6 |
| 5 | In1 | PB0 |
| 6 | In2 | PB1 |
| 7 | Vcc | 3V3 (atau PSU eksternal) |
| 8 | Out4 | PA7 |

**Ground bersama wajib:**

```
Power Supply (−) ──┬──► GND sensor (pin 2)
                   └──► GND STM32 (Black Pill)
                        └──► GND ST-Link
```

Tanpa ini: nilai acak 0..950, saturasi di state 2/3, ST-Link bisa
DEV_CONNECT_ERR.

---

## TODO berikutnya

- [ ] Ukur baseline kertas HITAM (pasangan putih/hitam per kanal)
- [ ] Konfirmasi apakah output modul inverted
- [ ] Tentukan threshold hitam/putih per kanal
- [ ] Verifikasi urutan fisik channel 0..15 (geser sensor di atas garis)
- [ ] Implementasi `Sensor_ReadRaw()` di `Firmware/LF_TED2026/Core/Src/sensor.c`
