# flash.ps1 — Flash firmware test DRIVER MOTOR ke STM32 via ST-Link
#
# Pakai:  .\flash.ps1
# Butuh: build\test_motor.elf (jalankan .\build.ps1 dulu)
#
# PERINGATAN: setelah flash, motor akan langsung berputar (fase auto).
# Angkat/ganjal roda dulu.

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_motor.elf"

if (-not (Test-Path $elf)) {
    throw "File $elf tidak ada. Jalankan .\build.ps1 dulu."
}

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path $cli)) {
    throw "STM32_Programmer_CLI.exe tidak ditemukan. Install STM32CubeProgrammer."
}

Write-Host "=== Cek ST-Link ===" -ForegroundColor Cyan
& $cli -l | Select-String -Pattern "ST-Link Probe|ST-LINK SN|No ST-Link"

Write-Host ""
Write-Host "PERINGATAN: motor akan berputar setelah reset. Angkat roda!" -ForegroundColor Yellow
Write-Host ""
Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Flash GAGAL. Cek:" -ForegroundColor Red
    Write-Host "  - ST-Link tercolok & driver terpasang"
    Write-Host "  - Kabel SWDIO->PA13, SWCLK->PA14, GND, 3V3"
    Write-Host "  - Board dapat daya (LED power nyala)"
    exit 1
}

Write-Host ""
Write-Host "=== Selesai ===" -ForegroundColor Green
Write-Host "Fase uji (lihat OLED kalau terpasang):"
Write-Host "  1-5  COAST + PIN WALK D0/D1/D2/D3  (uji koneksi 1 pin per fase)"
Write-Host "  6-11 MAJU/KIRI/KANAN/MUNDUR/REM"
Write-Host ""
Write-Host "Tombol: OK (PA5) = fase berikutnya, BACK (PA4) = jeda/lanjut."
Write-Host "Baca status via SWD:  .\read_motor.ps1"
