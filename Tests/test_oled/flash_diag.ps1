# flash_diag.ps1 — Flash firmware diagnostik + baca hasil scan I2C via SWD
#
# Pakai:  .\flash_diag.ps1
# Butuh: build\diag.elf (jalankan .\build_diag.ps1 dulu)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\diag.elf"
if (-not (Test-Path $elf)) { throw "File $elf tidak ada. Jalankan .\build_diag.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path $cli)) { throw "STM32_Programmer_CLI.exe tidak ditemukan." }

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|error|MCU Reset"

Write-Host ""
Write-Host "=== Baca hasil diagnosa via SWD ===" -ForegroundColor Cyan
Write-Host "(tunggu 2 detik agar firmware sempat scan I2C)"
Start-Sleep -Seconds 2

# Ambil alamat simbol dari ELF
$nm = Join-Path (Split-Path $cli) "..\..\..\..\..\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-nm.exe"
$nm = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName

$syms = @{}
if ($nm) {
    & $nm $elf | ForEach-Object {
        $p = $_ -split '\s+'
        if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
    }
}

if ($syms.Count -eq 0) {
    Write-Host "Tidak bisa baca simbol. Cek manual dengan LED PC13." -ForegroundColor Yellow
    exit 0
}

foreach ($k in @('g_scan_done','g_ack_100k','g_ack_400k','g_found_3c','g_found_3d','g_cmd_status','g_data_status','g_phase','g_loop')) {
    if (-not $syms.ContainsKey($k)) { continue }
    $addr = $syms[$k]
    $out = & $cli -c port=SWD -r32 ("0x{0:X8}" -f $addr) 0x1 2>&1 | Select-String -Pattern "^0x"
    $val = ($out -split '\s+')[-1]
    Write-Host ("  {0,-14} = {1}" -f $k, $val)
}

Write-Host ""
Write-Host "=== Arti hasil ===" -ForegroundColor Cyan
Write-Host "  g_found_3c=1  -> OLED ACK di 0x3C (normal)"
Write-Host "  g_found_3d=1  -> OLED ACK di 0x3D"
Write-Host "  keduanya 0    -> OLED TIDAK terdeteksi (wiring/daya/alamat)"
Write-Host "  g_cmd_status=0 -> tulis perintah OK (0=HAL_OK)"
Write-Host "  g_data_status=0 -> tulis data OK"
Write-Host ""
Write-Host "Perhatikan layar: bergantian putih tiap 3 detik (SH1106 lalu SSD1306)."
Write-Host "Kalau ada fase yang putih -> chipset-nya itu."
