# Dokumentasi Teknis & Master Blueprint: Robot Line Follower (TED 2026)

> **Catatan Konteks untuk AI Assistant (LLM Context Prompt):**  
> Dokumen ini adalah referensi arsitektur perangkat keras (*hardware*), skematik, *pinout mapping*, dan aturan *layout* PCB yang sudah difinalisasi untuk proyek robot *Line Follower* tim Mahasiswa S1 Teknik Elektro Universitas Diponegoro (Undip) dalam menghadapi kompetisi nasional **Tech Enthusiast Day (TED) 2026** di Sekolah Vokasi UGM. Saat membantu menulis kode program (*firmware*), melakukan *debugging*, atau mengevaluasi rangkaian, **wajib mengacu secara ketat pada konfigurasi pin dan spesifikasi komponen di bawah ini.**

---

## 1. Profil Proyek & Spesifikasi Kompetisi (TED 2026 - SV UGM)

* **Institusi Tim:** S1 Teknik Elektro, Universitas Diponegoro (Undip)
* **Kompetisi:** National Line Follower Competition - Tech Enthusiast Day (TED) 2026, SV UGM
* **Batas Regulasi Robot:**
  * **Dimensi Maksimal:** Panjang $20\text{ cm} \times$ Lebar $20\text{ cm} \times$ Tinggi $10\text{ cm}$.
  * **Berat Maksimal:** $3\text{ kg}$ (termasuk baterai).
  * **Tegangan Baterai Maksimal:** $12\text{ V}$ (toleransi $\pm 1.5\text{ V}$, sehingga baterai LiPo 3S $11.1\text{ V}$ nominal / $12.6\text{ V}$ *full charge* sah digunakan).
  * **Karakteristik Lintasan:** Garis standar dengan rintangan (*obstacle*), pencabangan, *checkpoint* (`CP 1` – `CP 4`), *Time Reduction Points*, serta **area *inverse line*** (garis putih di atas blok hitam seperti pada area tulisan *"TED 2026"* dan simbol petir).

---

## 2. Daftar Komponen Utama (Bill of Materials / BOM)

| Kategori | Nama Komponen | Spesifikasi Teknis & Catatan Implementasi |
| :--- | :--- | :--- |
| **Mikrokontroler (MCU)** | **STM32F411CEU6 "Black Pill V3"** | ARM Cortex-M4 $100\text{ MHz}$, $512\text{ KB}$ Flash, $128\text{ KB}$ RAM, Port USB Type-C. Dipasang menggunakan 2 baris *Female Header* $1\times20$ pin. |
| **Sensor Garis** | **Dariyan-X 16CH Line Sensor** | Sensor 16 *channel* fotodioda/IR dengan sistem *Dual 4-Channel Multiplexer* terintegrasi. Menggunakan konektor $1\times8$ pin. |
| **Driver Motor** | **ZK-5AD Dual DC Motor Driver** | Dual H-Bridge MOSFET $5\text{ A}$ kontinu (tegangan kerja $3.0\text{ V} - 14\text{ V}$). Memiliki 7 pin kontrol logika dan 6 terminal baut. |
| **Aktuator (Motor)** | **Gearbox V.2 Dariyan-X2 (ZRF)** | Sepasang motor DC *high-speed* dengan *gearbox* presisi dan roda *sponge/foam* khusus kompetisi *line follower*. |
| **Sumber Daya Utama** | **Baterai LiPo 3S ($11.1\text{ V}$)** | Kapasitas $1600\text{ mAh}$ $25\text{C}$ dengan konektor utama **XT60 Female** (di PCB dipasang **XT60 Male** *through-hole* pitch $7.2\text{ mm}$). |
| **Pemantau Baterai** | **Lipo Battery Buzzer Alarm** | Dipasang **eksternal** langsung ke soket putih *balance* **JST-XH** baterai (tidak menggunakan jalur pembagi tegangan ADC di PCB). |
| **Regulator Logika** | **Modul MP1584EN Fixed $3.3\text{ V}$** | *Step-down buck converter* DC-DC hingga $3\text{ A}$. Menurunkan tegangan baterai ($\sim 12\text{ V}$) menjadi jalur logika tunggal $+3.3\text{ V}$. |
| **Saklar Daya Utama** | **Rocker Switch KCD11 (2-Pin)** / **Slide Switch SS-12D00** | Memutus/menyambungkan jalur positif utama baterai ($+12\text{V\_RAW} \rightarrow +12\text{V\_SW}$). |
| **Layar Antarmuka** | **OLED $1.3\text{ inch}$ I2C Putih (4-Pin)** | Resolusi $128\times64$ piksel, menggunakan *driver chip* **SH1106** (bukan SSD1306). Urutan pin fisik: `VCC` - `GND` - `SCL` - `SDA`. |
| **Tombol Navigasi** | **5x Tactile Switch $6\times6\text{ mm}$ (4-Pin)** | Tipe *straight through-hole* (kaki lurus ke bawah, kode seri `TS-1187` / `TS-1101`). 4 tombol untuk menu (`UP`, `DOWN`, `OK`, `BACK`) & 1 tombol untuk `HARD RESET`. |
| **Kapasitor Filter** | **3x Kapasitor Pasif (Elco & Keramik)** | 1x Elco $470\ \mu\text{F}/25\text{ V}$ (`C1`), 1x Elco $100\ \mu\text{F}/16\text{ V}$ (`C2`), 1x Keramik $100\text{ nF}$ kode `104` (`C3`). |

---

## 3. Arsitektur Catu Daya & Penempatan Kapasitor Filter

Sistem kelistrikan dirancang menggunakan topologi **Single Logic Rail ($+3.3\text{ V}$)** untuk mencegah kerusakan pin STM32 akibat tegangan berlebih:

1. **Jalur Daya Kotor / Motor (`+12V_SW`):**
   * Mengalir dari pin positif konektor **XT60** $\rightarrow$ melewati **Saklar Utama** $\rightarrow$ menuju terminal baut `(+)` **Driver Motor ZK-5AD** dan pin `IN+` **Modul MP1584EN**.
   * **Filter `C1` (Elco $470\ \mu\text{F} / 25\text{ V}$):** Dipasang paralel antara `+12V_SW` dan `GND`, diletakkan **sedekat mungkin dengan terminal baut daya ZK-5AD** untuk meredam lonjakan arus (*current spike* & *voltage dip*) saat motor berakselerasi mendadak.
2. **Jalur Daya Bersih / Logika (`+3V3_SYS`):**
   * Keluar dari pin `OUT+` **Modul MP1584EN Fixed $3.3\text{ V}$** (kedua lubang `OUT+` pada modul digabung menjadi satu jalur).
   * Menyuplai daya ke:
     * Pin `3V3` **STM32F411 Black Pill** (masuk melalui pin kanan atas: **Pin 23 `3V3` & Pin 22 `GND`**).
     * Pin `Vcc` **Kepala Sensor 16CH Dariyan-X**.
     * Pin `VCC` **Layar OLED $1.3\text{"}$** (diambil dari pin kiri bawah STM32: **Pin 20 `3V3` & Pin 19 `GND`** agar jalur PCB tidak menumpuk).
   * **Filter `C2` (Elco $100\ \mu\text{F} / 16\text{ V}$):** Dipasang tepat di pintu keluar `OUT+` dan `OUT-` modul **MP1584EN** sebagai tandon daya (*bulk capacitor*) frekuensi rendah.
   * **Filter `C3` (Keramik $100\text{ nF}$ / Kode `104`):** Dipasang paralel mendampingi Elco, diletakkan **mepet di samping Pin 23 (`3V3`) dan Pin 22 (`GND`) STM32 sisi kanan atas** sebagai *decoupling capacitor* untuk membuang *noise* frekuensi tinggi ($\sim 1\text{ MHz}$ *switching* regulator & $100\text{ MHz}$ *clock* CPU) ke `GND`.
3. **Isolasi Regulator Driver Motor:**
   * Pin `3V3` pada header kontrol 7-pin modul **ZK-5AD** merupakan *output* tegangan $3.3\text{ V}$ internal driver. Pin ini **WAJIB DIBIARKAN KOSONG (`NC` / *No Connect*)** dan dilarang keras disambungkan ke jalur `+3V3_SYS` agar tidak terjadi tabrakan arus antar-regulator.

---

## 4. Tabel Pemetaan Pin (Master Pinout Mapping)

Tabel ini adalah acuan mutlak untuk pembuatan skematik di EasyEDA Pro dan penulisan program (*firmware*):

| Komponen / Modul | Nama Pin Komponen | Nama NetLabel (EasyEDA) | Pin STM32F411 (Black Pill) | Konfigurasi Software / Peripheral |
| :--- | :--- | :--- | :--- | :--- |
| **Sensor Dariyan-X (Pin 1)** | `Out1` | `SENS_OUT1` | **`PA4`** | `ADC1_IN4` (Baca grup sensor 1–4) |
| **Sensor Dariyan-X (Pin 2)** | `Gnd` | `GND` | **`G` (`GND`)** | Ground Sistem |
| **Sensor Dariyan-X (Pin 3)** | `Out2` | `SENS_OUT2` | **`PA5`** | `ADC1_IN5` (Baca grup sensor 5–8) |
| **Sensor Dariyan-X (Pin 4)** | `Out3` | `SENS_OUT3` | **`PA6`** | `ADC1_IN6` (Baca grup sensor 9–12) |
| **Sensor Dariyan-X (Pin 5)** | `In1` | `MUX_IN1` | **`PB0`** | `GPIO_Output` (Selektor Multiplexer Bit 0) |
| **Sensor Dariyan-X (Pin 6)** | `In2` | `MUX_IN2` | **`PB1`** | `GPIO_Output` (Selektor Multiplexer Bit 1) |
| **Sensor Dariyan-X (Pin 7)** | `Vcc` | `+3V3_SYS` | **`3V3`** | Suplai Daya Sensor $+3.3\text{ V}$ |
| **Sensor Dariyan-X (Pin 8)** | `Out4` | `SENS_OUT4` | **`PA7`** | `ADC1_IN7` (Baca grup sensor 13–16) |
| **Driver ZK-5AD (Pin 1)** | `GND` | `GND` | **`G` (`GND`)** | Referensi Ground Sinyal PWM |
| **Driver ZK-5AD (Pin 2)** | `3V3` | *(Tidak Disambung / NC)* | **NC** | **JANGAN DISAMBUNG!** |
| **Driver ZK-5AD (Pin 3)** | `D0` | `DRV_D0` | **`PA8`** | `TIM1_CH1` (PWM Motor 1 / Kiri - IN1) |
| **Driver ZK-5AD (Pin 4)** | `D1` | `DRV_D1` | **`PA9`** | `TIM1_CH2` (PWM Motor 1 / Kiri - IN2) |
| **Driver ZK-5AD (Pin 5)** | `D2` | `DRV_D2` | **`PA10`** | `TIM1_CH3` (PWM Motor 2 / Kanan - IN1) |
| **Driver ZK-5AD (Pin 6)** | `D3` | `DRV_D3` | **`PA11`** | `TIM1_CH4` (PWM Motor 2 / Kanan - IN2) |
| **Driver ZK-5AD (Pin 7)** | `VT` | *(Tidak Disambung / NC)* | **NC** | Tidak digunakan |
| **OLED 1.3" SH1106 (Pin 1)** | `VCC` | `+3V3_SYS` | **`3V3` (Pin 20)** | Suplai Daya Layar $+3.3\text{ V}$ |
| **OLED 1.3" SH1106 (Pin 2)** | `GND` | `GND` | **`GND` (Pin 19)** | Ground Layar |
| **OLED 1.3" SH1106 (Pin 3)** | `SCL` | `OLED_SCL` | **`PB6`** | `I2C1_SCL` (Hardware I2C1 Clock) |
| **OLED 1.3" SH1106 (Pin 4)** | `SDA` | `OLED_SDA` | **`PB7`** | `I2C1_SDA` (Hardware I2C1 Data) |
| **Tombol Menu 1 (UP)** | Pin 1–2 (Input), Pin 3–4 (GND) | `BTN_UP` | **`PA0`** | `INPUT_PULLUP` (Aktif `LOW` saat ditekan) |
| **Tombol Menu 2 (DOWN)** | Pin 1–2 (Input), Pin 3–4 (GND) | `BTN_DOWN` | **`PA1`** | `INPUT_PULLUP` (Aktif `LOW` saat ditekan) |
| **Tombol Menu 3 (OK/ENTER)** | Pin 1–2 (Input), Pin 3–4 (GND) | `BTN_OK` | **`PA2`** | `INPUT_PULLUP` (Aktif `LOW` saat ditekan) |
| **Tombol Menu 4 (BACK)** | Pin 1–2 (Input), Pin 3–4 (GND) | `BTN_BACK` | **`PA3`** | `INPUT_PULLUP` (Aktif `LOW` saat ditekan) |
| **Tombol Ke-5 (HARD RESET)**| Pin 1–2 (Input), Pin 3–4 (GND) | `BTN_RST` | **`R` (`NRST` - Pin 36)**| Hardware Reset (Langsung *restart* MCU) |

---

## 5. Diagram Skematik Blok (ASCII Master Blueprint)

```text
========================================================================================================
[BLOK 1: SUMBER DAYA, SAKLAR UTAMA & FILTER]             [BLOK 2: BUCK CONVERTER & FILTER LOGIKA 3.3V]
========================================================================================================

  J_XT60 (XT60 Male di PCB - Pitch 7.2mm)                  U_BUCK (Modul MP1584EN Fixed 3.3V)
  ┌──────────┐        SW_PWR (Saklar KCD11 2-Pin)          ┌──────────────────────┐
  │ 1 (POS+) ├─────────┬──/ ──┬───────────────────────────►│ IN+             OUT+ ├───┬───┬───► +3V3_SYS
  │          │         │      │                            │                      │   │   │
  │ 2 (NEG-) ├───┐     │      ├───┐                        │                      │  ┌┴┐ ┌┴┐
  └──────────┘   │     │      │  ┌┴┐ C1 (Elco 470uF/25V)   │                      │  │+│ │ │ C3 (Keramik
                 │     │      │  │+│ [Dekat Baut ZK-5AD]   │                      │  └┬┘ └┬┘  100nF/104)
                ─┴─    │      │  └┬┘                       │                      │   │   │  [Dekat Pin
                GND    │      │  ─┴─                       │ IN-             OUT- ├─┐ │   │   23 & 22 MCU]
                       │      │  GND                       └──────────────────────┘ │ │   │
                       │      │                                      ▲              │ │   │
                       │      └──► +12V_SW (Jalur Tebal 2mm          └──────────────┴─┴───┴───► ─┴─ GND
                       │                    ke Driver ZK-5AD)                             C2 (Elco 100uF/16V)


========================================================================================================
[BLOK 3: MIKROKONTROLER - MODUL STM32F411CEU6 "BLACK PILL" (2x Baris Header 1x20)]
========================================================================================================

                 DERET PIN KIRI (Pin 1 - 20)                           DERET PIN KANAN (Pin 21 - 40)
                     ┌──────────────┐                                   ┌──────────────┐
          (Kosong) x─┤ 1  PB12      │                                   │ 5V        21 ├─x (Kosong)
          (Kosong) x─┤ 2  PB13      │                                   │ GND       22 ├───► GND ( + C3 )
          (Kosong) x─┤ 3  PB14      │                                   │ 3V3       23 ◄──── +3V3_SYS ( + C3 )
          (Kosong) x─┤ 4  PB15      │                          (Kosong) │ PB10      24 ├─x
      DRV_D0 (M1) ◄──┤ 5  PA8       │                          (Kosong) │ PB2       25 ├─x
      DRV_D1 (M1) ◄──┤ 6  PA9       │                       MUX_IN2 ◄───┤ PB1       26 │
      DRV_D2 (M2) ◄──┤ 7  PA10      │                       MUX_IN1 ◄───┤ PB0       27 │
      DRV_D3 (M2) ◄──┤ 8  PA11      │                     SENS_OUT4 ───►│ PA7       28 │
          (Kosong) x─┤ 9  PA12      │                     SENS_OUT3 ───►│ PA6       29 │
          (Kosong) x─┤ 10 PA15      │                     SENS_OUT2 ───►│ PA5       30 │
          (Kosong) x─┤ 11 PB3       │                     SENS_OUT1 ───►│ PA4       31 │
          (Kosong) x─┤ 12 PB4       │                      BTN_BACK ───►│ PA3       32 │
          (Kosong) x─┤ 13 PB5       │                      BTN_OK   ───►│ PA2       33 │
         OLED_SCL ◄──┤ 14 PB6       │                      BTN_DOWN ───►│ PA1       34 │
         OLED_SDA ◄──┤ 15 PB7       │                      BTN_UP   ───►│ PA0       35 │
          (Kosong) x─┤ 16 PB8       │                      BTN_RST  ───►│ NRST (R)  36 │
          (Kosong) x─┤ 17 PB9       │                                   │ PC15      37 ├─x (Kosong)
          (Kosong) x─┤ 18 5V        │                                   │ PC14      38 ├─x (Kosong)
  (Ke OLED)   GND ◄──┤ 19 GND       │                                   │ PC13      39 ├─x (LED Internal)
  (Ke OLED)  +3V3 ──►│ 20 3V3       │                                   │ VBAT      40 ├─x (Kosong)
                     └──────────────┘                                   └──────────────┘
```

---

## 6. Standar Aturan Desain Jalur PCB (EasyEDA Pro Routing Rules)

Konversi satuan dasar di EasyEDA Pro: **$10\text{ mil} = 0.254\text{ mm}$** ($1\text{ inch} = 1000\text{ mil} = 25.4\text{ mm}$).

| Jenis Jalur (Net) | Alur Koneksi | Ukuran dalam `mm` | Ukuran dalam `mil` | Alasan Teknis |
| :--- | :--- | :--- | :--- | :--- |
| **Daya Utama & Motor (`+12V_RAW`, `+12V_SW`)** | XT60 $\rightarrow$ Saklar $\rightarrow$ ZK-5AD & Input MP1584EN | **$1.5\text{ mm} - 2.5\text{ mm}$** | **$60\text{ mil} - 100\text{ mil}$** | Menahan lonjakan arus (*stall current*) dua motor Gearbox ZRF hingga $4\text{ A} - 6\text{ A}$ tanpa panas/putus. |
| **GND Arus Tinggi** | Negatif XT60 $\leftrightarrow$ GND ZK-5AD & `IN-` MP1584EN | **$1.5\text{ mm} - 2.5\text{ mm}$** | **$60\text{ mil} - 100\text{ mil}$** | Jalur balik arus motor wajib sama besar dengan jalur positifnya. |
| **Daya Logika (`+3V3_SYS`) & GND Logika** | `OUT+` MP1584EN $\rightarrow$ STM32, Sensor Dariyan-X | **$0.6\text{ mm} - 1.0\text{ mm}$** | **$24\text{ mil} - 40\text{ mil}$** | Menjaga kestabilan tegangan referensi ADC STM32 dan suplai 16 LED IR sensor. |
| **Daya OLED (`VCC` & `GND` OLED)** | Pin 20/19 STM32 $\rightarrow$ Pin 1/2 OLED $1.3\text{"}$ | **$0.5\text{ mm} - 0.6\text{ mm}$** | **$20\text{ mil} - 24\text{ mil}$** | Beban arus layar kecil ($\sim 30\text{ mA}$), mudah dirutekan di sela-sela pin. |
| **Sinyal Data & Tombol** | 6 jalur sensor, 4 jalur PWM, `SCL`/`SDA`, 5 tombol | **$0.25\text{ mm} - 0.3\text{ mm}$** | **$10\text{ mil} - 12\text{ mil}$** | Arus logika sangat kecil (mikroampere), ukuran *default* `10 mil` paling rapi dan mudah diselipkan. |
| **Hamparan Ground Utama** | Seluruh ruang kosong Top & Bottom Layer | **Copper Area (`Shift+P`)** | **Polygon Pour `GND`** | Meredam *noise* elektromagnetik (EMI) motor dan menjadi *heatsink* alami bagi MP1584EN. |

---

## 7. Panduan Koding & Perangkat Lunak (Firmware Guidelines)

Bagian ini wajib diperhatikan saat menulis program di Arduino IDE (`STM32duino`) atau STM32CubeIDE:

### A. Pembacaan Sensor 16CH Dariyan-X (Dual 4-Ch Multiplexer)
Kepala sensor menggunakan 2 pin selektor digital (`PB0` untuk `In1`, `PB1` untuk `In2`) dan 4 pin pembacaan analog (`PA4`=`Out1`, `PA5`=`Out2`, `PA6`=`Out3`, `PA7`=`Out4`).
* Setiap perubahan kombinasi bit pada `(In2, In1)` yaitu `00`, `01`, `10`, dan `11`, berikan jeda *settling time* singkat (misal `delayMicroseconds(5)` s.d. `10`) sebelum membaca keempat kanal ADC (`PA4`–`PA7`) secara berurutan sehingga didapatkan total $4 \times 4 = 16$ nilai sensor.

### B. Layar OLED 1.3" (Wajib Driver SH1106)
* Layar OLED $1.3\text{"}$ **menggunakan chip SH1106, BUKAN SSD1306**. Menggunakan library `Adafruit_SSD1306` akan membuat layar gelap total meski kabel benar.
* **Rekomendasi Library:** Gunakan **`U8g2`** dengan konstruktor:
  ```cpp
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
  ```
  Atau gunakan library **`Adafruit_SH1106`** dengan alamat I2C standar **`0x3C`**.

### C. Kontrol Driver Motor ZK-5AD (Dual H-Bridge)
Logika pengendalian kecepatan dan arah putaran per motor menggunakan sinyal PWM pada `PA8`–`PA11` (`TIM1`):
* **Maju (Forward):** Pin 1 (`D0` / `D2`) diberi sinyal `PWM`, Pin 2 (`D1` / `D3`) diberi `LOW (0)`.
* **Mundur (Reverse):** Pin 1 (`D0` / `D2`) diberi `LOW (0)`, Pin 2 (`D1` / `D3`) diberi sinyal `PWM`.
* **Rem Aktif (Brake):** Kedua pin diberi logika `HIGH (1)`.
* **Bebas (Coast/Stop):** Kedua pin diberi logika `LOW (0)`.

### D. Fitur Wajib di Menu Robot (Strategi Lintasan TED 2026)
1. **Kalibrasi Sensor Otomatis/Manual:** Menyimpan nilai *Threshold* (batas tengah hitam-putih) ke memori Flash/EEPROM emulated STM32.
2. **Parameter PID Control:** Pengaturan nilai $K_p$, $K_i$, $K_d$, serta *Base Speed* dan *Max Speed*.
3. **Path Planning / Indexing Memory:** Untuk menangani *Checkpoint* (`CP 1` – `CP 4`), persimpangan, dan **fitur `Invert Line` (Garis Hitam $\leftrightarrow$ Garis Putih)** saat melewati blok hitam bertuliskan *"TED 2026"* dan rintangan petir.