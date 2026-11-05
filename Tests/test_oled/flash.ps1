# flash.ps1 — Flash firmware test OLED ke STM32 via ST-Link
#
# Pakai:  .\flash.ps1
# Butuh: build\test_oled.elf (jalankan .\build.ps1 dulu)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_oled.elf"

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
Write-Host "OLED seharusnya sudah menampilkan layar demo."
Write-Host "Kalau gelap, tekan tombol RESET di board."
