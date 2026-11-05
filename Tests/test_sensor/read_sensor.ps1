# read_sensor.ps1 — Baca hasil scan sensor via SWD (tanpa UART)
#
# Pakai:  .\read_sensor.ps1
# Butuh: build\test_sensor.elf (jalankan .\build.ps1 dulu) + board ter-flash
#
# Membaca g_raw[16], g_min[16], g_max[16], g_scan_count, g_scan_us, g_adc_err.
# Memakai mode=hotplug supaya MCU TIDAK di-reset saat dibaca.

param(
    [int]$Samples = 1,
    [int]$IntervalMs = 500
)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_sensor.elf"
if (-not (Test-Path $elf)) { throw "ELF tidak ada: $elf. Jalankan .\build.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path $cli)) { throw "STM32_Programmer_CLI.exe tidak ditemukan." }

$nm = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue |
       Select-Object -First 1).FullName
if (-not $nm) { throw "arm-none-eabi-nm.exe tidak ditemukan." }

# ---------- Peta simbol g_* ----------
$syms = @{}
& $nm $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') {
        $syms[$p[2]] = [Convert]::ToUInt32($p[0], 16)
    }
}

function Read-Block([uint32]$addr, [int]$bytes) {
    $out = & $cli -c port=SWD mode=hotplug -r32 ("0x{0:X8}" -f $addr) ("0x{0:X}" -f $bytes) 2>&1
    $words = @()
    foreach ($line in $out) {
        if ($line -match '^(0x[0-9A-Fa-f]{8})\s*:\s*(.+)$') {
            foreach ($v in ($matches[2].Trim() -split '\s+')) {
                if ($v -match '^[0-9A-Fa-f]{8}$') { $words += [Convert]::ToUInt32($v, 16) }
            }
        }
    }
    return $words
}

# ---------- Cek koneksi SWD dulu, beri pesan jelas kalau gagal ----------
$probe = & $cli -l 2>&1
if ($probe -match 'DEV_CONNECT_ERR' -or -not ($probe -match 'ST-LINK FW\s*:\s*\S')) {
    Write-Host ""
    Write-Host "ST-Link tidak bisa diakses (DEV_CONNECT_ERR / FW kosong)." -ForegroundColor Red
    Write-Host ""
    Write-Host "Coba langkah ini:" -ForegroundColor Yellow
    Write-Host "  1. Cabut ST-Link dari USB, tunggu 3 detik, colok lagi."
    Write-Host "  2. Tutup STM32CubeIDE / STM32CubeProgrammer kalau sedang terbuka."
    Write-Host "  3. Pastikan GND ST-Link tersambung ke GND board."
    Write-Host "  4. Kalau board diberi power supply eksternal, GND-nya WAJIB disambung"
    Write-Host "     ke GND ST-Link (ground bersama)."
    Write-Host ""
    exit 1
}

function Read-U16Array([string]$name, [int]$count) {
    if (-not $syms.ContainsKey($name)) { return @() }
    $words = Read-Block $syms[$name] ($count * 2)
    $vals = @()
    foreach ($w in $words) {
        $vals += ($w -band 0xFFFF)
        $vals += (($w -shr 16) -band 0xFFFF)
    }
    if ($vals.Count -lt $count) { return @() }
    return $vals[0..($count - 1)]
}

function Read-U32([string]$name) {
    if (-not $syms.ContainsKey($name)) { return $null }
    $w = Read-Block $syms[$name] 4
    if ($w.Count -eq 0) { return $null }
    return $w[0]
}

for ($n = 0; $n -lt $Samples; $n++) {
    if ($Samples -gt 1) { Write-Host ("--- sampel {0}/{1} ---" -f ($n + 1), $Samples) -ForegroundColor DarkGray }

    $raw = Read-U16Array 'g_raw' 16
    $min = Read-U16Array 'g_min' 16
    $max = Read-U16Array 'g_max' 16
    $cnt = Read-U32 'g_scan_count'
    $us  = Read-U32 'g_scan_us'
    $err = Read-U32 'g_adc_err'

    if ($raw.Count -ne 16) {
        Write-Host ""
        Write-Host "Gagal membaca g_raw[] dari MCU (koneksi SWD terputus di tengah)." -ForegroundColor Red
        Write-Host "Cabut-colok ST-Link lalu jalankan lagi." -ForegroundColor Yellow
        exit 1
    }

    Write-Host ""
    Write-Host "=== RAW 16 kanal (indeks = group*4 + mux_state) ===" -ForegroundColor Cyan
    Write-Host "  idx :  g0   g1   g2   g3  |  g4   g5   g6   g7  |  g8   g9  g10  g11  | g12  g13  g14  g15"
    $line = "  raw : "
    for ($i = 0; $i -lt 16; $i++) {
        $line += ("{0,4}" -f $raw[$i])
        if (($i % 4) -eq 3 -and $i -lt 15) { $line += " |" }
    }
    Write-Host $line

    if ($min.Count -eq 16 -and $max.Count -eq 16) {
        $line = "  min : "
        for ($i = 0; $i -lt 16; $i++) {
            $line += ("{0,4}" -f $min[$i])
            if (($i % 4) -eq 3 -and $i -lt 15) { $line += " |" }
        }
        Write-Host $line
        $line = "  max : "
        for ($i = 0; $i -lt 16; $i++) {
            $line += ("{0,4}" -f $max[$i])
            if (($i % 4) -eq 3 -and $i -lt 15) { $line += " |" }
        }
        Write-Host $line
    }

    Write-Host ""
    Write-Host ("  scan_count = {0}" -f $cnt)
    Write-Host ("  scan_us    = {0} us  (satu siklus 4 mux x 4 kanal)" -f $us)
    Write-Host ("  adc_err    = {0}" -f $err)

    if ($cnt -eq 0) {
        Write-Host ""
        Write-Host "  PERINGATAN: scan_count = 0 -> firmware mungkin belum jalan / board belum di-flash." -ForegroundColor Yellow
    }
    if ($err -gt 0) {
        Write-Host "  PERINGATAN: ada timeout ADC (adc_err > 0)." -ForegroundColor Yellow
    }

    if ($n -lt $Samples - 1) { Start-Sleep -Milliseconds $IntervalMs }
}
