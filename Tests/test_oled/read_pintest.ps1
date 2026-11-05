# read_pintest.ps1 — Flash pin_test + baca semua hasil via SWD
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\pintest.elf"
if (-not (Test-Path $elf)) { throw "Jalankan .\build_pintest.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$nm  = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"

Start-Sleep -Seconds 3

# Ambil alamat simbol
$syms = @{}
& $nm $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

function Read-Sym($name) {
    if (-not $syms.ContainsKey($name)) { return $null }
    $a = "0x{0:X8}" -f $syms[$name]
    $out = & $cli -c port=SWD -r32 $a 0x1 2>&1 | Select-String -Pattern "^0x"
    if (-not $out) { return $null }
    return ($out -split '\s+')[-1]
}

Write-Host ""
Write-Host "=== Hasil uji pin ===" -ForegroundColor Cyan

$t1 = Read-Sym 'g_t1_pullup'
$t2 = Read-Sym 'g_t2_nopull'
$t3 = Read-Sym 'g_t3_drive_hi'
$t4 = Read-Sym 'g_t4_drive_lo'

function Bits($hex) {
    if (-not $hex) { return "?" }
    $v = [Convert]::ToUInt32($hex,16)
    $scl = ($v -shr 6) -band 1
    $sda = ($v -shr 7) -band 1
    return ("SCL=" + $scl + " SDA=" + $sda)
}

Write-Host ("  Tes1 input+pullup   : $t1  -> " + (Bits $t1))
Write-Host ("  Tes2 input no-pull  : $t2  -> " + (Bits $t2))
Write-Host ("  Tes3 drive HIGH     : $t3  -> " + (Bits $t3))
Write-Host ("  Tes4 drive LOW      : $t4  -> " + (Bits $t4))

Write-Host ""
Write-Host "=== Scan I2C 0x08..0x77 ===" -ForegroundColor Cyan
$lo = Read-Sym 'g_scan_lo'
$hi = Read-Sym 'g_scan_hi'
$cnt = Read-Sym 'g_scan_count'
$first = Read-Sym 'g_first_addr'
Write-Host ("  jumlah device = $cnt")
Write-Host ("  alamat pertama = $first")
Write-Host ("  bitmap lo (0x08-0x3F) = $lo")
Write-Host ("  bitmap hi (0x40-0x77) = $hi")

if ($cnt -and [Convert]::ToUInt32($cnt,16) -gt 0) {
    Write-Host ""
    Write-Host "  Device ditemukan di:" -ForegroundColor Green
    $v = [Convert]::ToUInt32($lo,16)
    for ($i = 0; $i -lt 32; $i++) { if (($v -shr $i) -band 1) { Write-Host ("    0x{0:X2}" -f (0x08 + $i)) } }
    $v = [Convert]::ToUInt32($hi,16)
    for ($i = 0; $i -lt 32; $i++) { if (($v -shr $i) -band 1) { Write-Host ("    0x{0:X2}" -f (0x40 + $i)) } }
} else {
    Write-Host ""
    Write-Host "  TIDAK ADA device I2C sama sekali di bus." -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Interpretasi ===" -ForegroundColor Cyan
$v1 = if ($t1) { [Convert]::ToUInt32($t1,16) } else { 0 }
$v2 = if ($t2) { [Convert]::ToUInt32($t2,16) } else { 0 }
$v3 = if ($t3) { [Convert]::ToUInt32($t3,16) } else { 0 }
$v4 = if ($t4) { [Convert]::ToUInt32($t4,16) } else { 0 }

$scl1 = ($v1 -shr 6) -band 1; $sda1 = ($v1 -shr 7) -band 1
$scl2 = ($v2 -shr 6) -band 1; $sda2 = ($v2 -shr 7) -band 1
$scl3 = ($v3 -shr 6) -band 1; $sda3 = ($v3 -shr 7) -band 1
$scl4 = ($v4 -shr 6) -band 1; $sda4 = ($v4 -shr 7) -band 1

if ($scl1 -eq 1 -and $sda1 -eq 1) {
    Write-Host "  [OK] Pull-up internal bisa menarik kedua line HIGH." -ForegroundColor Green
    Write-Host "       Berarti tidak ada short ke GND di jalur SCL/SDA."
} else {
    Write-Host "  [!!] Dengan pull-up internal, line TETAP LOW." -ForegroundColor Red
    Write-Host "       Ada yang menahan ke GND: short, atau OLED rusak, atau VCC/GND tertukar."
}

if ($scl3 -eq 1 -and $sda3 -eq 1) {
    Write-Host "  [OK] MCU bisa drive kedua line HIGH." -ForegroundColor Green
} else {
    Write-Host "  [!!] MCU drive HIGH tapi line tetap LOW -> short ke GND." -ForegroundColor Red
}

if ($scl4 -eq 0 -and $sda4 -eq 0) {
    Write-Host "  [OK] MCU bisa drive kedua line LOW." -ForegroundColor Green
} else {
    Write-Host "  [!!] MCU drive LOW tapi line tetap HIGH -> jalur putus." -ForegroundColor Red
}

if ($scl2 -eq 1 -and $sda2 -eq 1) {
    Write-Host "  [OK] Ada pull-up EKSTERNAL di modul (line HIGH walau pull internal mati)." -ForegroundColor Green
} elseif ($scl1 -eq 1 -and $sda1 -eq 1) {
    Write-Host "  [--] Tanpa pull-up internal line LOW -> modul TIDAK punya pull-up." -ForegroundColor Yellow
    Write-Host "       Ini normal untuk banyak modul OLED (pull-up ada di board)."
}
