# read_diag.ps1 — Flash firmware diagnostik + baca hasil karakterisasi
#
# Pakai:  .\read_diag.ps1
# Butuh: build\test_sensor_diag.elf (jalankan .\build_diag.ps1 dulu)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_sensor_diag.elf"
if (-not (Test-Path $elf)) { throw "ELF tidak ada: $elf. Jalankan .\build_diag.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path $cli)) { throw "STM32_Programmer_CLI.exe tidak ditemukan." }

$nm = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue |
       Select-Object -First 1).FullName
if (-not $nm) { throw "arm-none-eabi-nm.exe tidak ditemukan." }

# ---------- Cek probe ----------
$probe = & $cli -l 2>&1
if ($probe -match 'DEV_CONNECT_ERR' -or -not ($probe -match 'ST-LINK FW\s*:\s*\S')) {
    Write-Host "ST-Link tidak bisa diakses. Cabut-colok ST-Link lalu coba lagi." -ForegroundColor Red
    exit 1
}

# ---------- Flash ----------
Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"
if ($LASTEXITCODE -ne 0) { throw "Flash gagal." }

Write-Host "Menunggu firmware selesai karakterisasi (~3 detik)..." -ForegroundColor DarkGray
Start-Sleep -Seconds 4

# ---------- Peta simbol ----------
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

$done = Read-U32 'g_diag_done'
if ($done -ne 1) {
    Write-Host "PERINGATAN: g_diag_done != 1 (firmware mungkin belum selesai)." -ForegroundColor Yellow
}

$dmin  = Read-U16Array 'g_dmin' 16
$dmax  = Read-U16Array 'g_dmax' 16
$dmean = Read-U16Array 'g_dmean' 16
$fmin  = Read-U16Array 'g_fix_min' 4
$fmax  = Read-U16Array 'g_fix_max' 4
$vmin  = Read-U16Array 'g_vref_min' 1
$vmax  = Read-U16Array 'g_vref_max' 1
$vmean = Read-U16Array 'g_vref_mean' 1
$pmin  = Read-U16Array 'g_poll_min' 4
$pmax  = Read-U16Array 'g_poll_max' 4
$pmean = Read-U16Array 'g_poll_mean' 4

Write-Host ""
Write-Host "=== UJI 0: VREFINT (referensi internal MCU, tidak tergantung sensor) ===" -ForegroundColor Cyan
if ($vmean.Count -eq 1) {
    $vspread = $vmax[0] - $vmin[0]
    Write-Host ("  mean={0}  min={1}  max={2}  spread={3}" -f $vmean[0], $vmin[0], $vmax[0], $vspread)
    Write-Host "  (nilai normal ~1500 @ Vdda 3.3V; spread kecil = ADC sehat)"
    if ($vspread -le 50) {
        Write-Host "  -> ADC + suplai MCU SEHAT. Masalah ada di sisi SENSOR." -ForegroundColor Green
    } else {
        Write-Host "  -> ADC/suplai MCU TIDAK STABIL. Cek Vdda & ground." -ForegroundColor Red
    }
} else {
    Write-Host "  (gagal baca)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== UJI 0b: POLLING tanpa DMA, mux ditahan state 0, 200 baca ===" -ForegroundColor Cyan
if ($pmean.Count -eq 4) {
    for ($g = 0; $g -lt 4; $g++) {
        $spread = $pmax[$g] - $pmin[$g]
        $flag = if ($spread -gt 200) { "  <-- TIDAK STABIL" } else { "  OK" }
        Write-Host ("  Out{0} (PA{1}) : mean={2,5}  min={3,5}  max={4,5}  spread={5,5}{6}" -f ($g+1), (4+$g), $pmean[$g], $pmin[$g], $pmax[$g], $spread, $flag)
    }
    $pollBad = @(0..3 | Where-Object { ($pmax[$_] - $pmin[$_]) -gt 200 }).Count
    if ($pollBad -gt 0) {
        Write-Host "  -> Polling juga kacau. Masalah HARDWARE (ground/sensor)." -ForegroundColor Red
    } else {
        Write-Host "  -> Polling STABIL." -ForegroundColor Green
    }
} else {
    Write-Host "  (gagal baca)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== UJI 1: mux DITAHAN di state 0, 200 baca ===" -ForegroundColor Cyan
Write-Host "  (kalau spread di sini besar -> masalah sensor/ADC, bukan settling mux)"
if ($fmin.Count -eq 4 -and $fmax.Count -eq 4) {
    for ($g = 0; $g -lt 4; $g++) {
        $spread = $fmax[$g] - $fmin[$g]
        $flag = if ($spread -gt 200) { "  <-- TIDAK STABIL" } else { "" }
        Write-Host ("  Out{0} (PA{1}) : min={2,5}  max={3,5}  spread={4,5}{5}" -f ($g+1), (4+$g), $fmin[$g], $fmax[$g], $spread, $flag)
    }
} else {
    Write-Host "  (gagal baca)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== UJI 2: karakterisasi per state mux (settle 5 ms, 100 sampel) ===" -ForegroundColor Cyan
Write-Host "  idx = group*4 + state"
Write-Host ""
Write-Host "  idx :  mean   min   max  spread"
for ($i = 0; $i -lt 16; $i++) {
    if ($dmean.Count -eq 16 -and $dmin.Count -eq 16 -and $dmax.Count -eq 16) {
        $spread = $dmax[$i] - $dmin[$i]
        $flag = ""
        if ($dmax[$i] -ge 4090) { $flag = "  <-- SATURASI" }
        elseif ($spread -gt 300) { $flag = "  <-- noisy" }
        Write-Host ("  {0,3} : {1,5} {2,5} {3,5} {4,6}{5}" -f $i, $dmean[$i], $dmin[$i], $dmax[$i], $spread, $flag)
    }
}

Write-Host ""
Write-Host "=== Interpretasi ===" -ForegroundColor Cyan
$sat = 0
if ($dmax.Count -eq 16) { $sat = @($dmax | Where-Object { $_ -ge 4090 }).Count }
Write-Host ("  kanal saturasi (max >= 4090) : {0}/16" -f $sat)
if ($fmin.Count -eq 4 -and $fmax.Count -eq 4) {
    $bigSpread = @(0..3 | Where-Object { ($fmax[$_] - $fmin[$_]) -gt 200 }).Count
    Write-Host ("  output tidak stabil (spread>200) : {0}/4" -f $bigSpread)
    if ($bigSpread -ge 3) {
        Write-Host ""
        Write-Host "  KESIMPULAN: nilai tidak stabil BAHKAN tanpa ganti mux." -ForegroundColor Yellow
        Write-Host "  -> Masalah di sensor/modul atau suplai, bukan timing mux." -ForegroundColor Yellow
    }
}
