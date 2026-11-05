# probe_pins.ps1 — Baca kondisi pin I2C langsung dari register via SWD
#
# GPIOB base = 0x40020400
#   MODER  +0x00  (bit 12-13 = PB6, bit 14-15 = PB7)
#   IDR    +0x10  (bit 6 = PB6/SCL, bit 7 = PB7/SDA)
#   PUPDR  +0x0C  (bit 12-13 = PB6, bit 14-15 = PB7)

$ErrorActionPreference = 'Stop'
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

function Read-Reg($addr) {
    $out = & $cli -c port=SWD -r32 ("0x{0:X8}" -f $addr) 0x1 2>&1 | Select-String -Pattern "^0x"
    if (-not $out) { return $null }
    return ($out -split '\s+')[-1]
}

Write-Host "=== Register GPIOB ===" -ForegroundColor Cyan
$moder = Read-Reg 0x40020400
$pupdr = Read-Reg 0x4002040C
$idr   = Read-Reg 0x40020410
$odr   = Read-Reg 0x40020414

Write-Host ("  MODER = $moder")
Write-Host ("  PUPDR = $pupdr")
Write-Host ("  IDR   = $idr")
Write-Host ("  ODR   = $odr")
Write-Host ""

if ($idr) {
    $v = [Convert]::ToUInt32($idr, 16)
    $scl = ($v -shr 6) -band 1
    $sda = ($v -shr 7) -band 1
    Write-Host "=== Kondisi pin ===" -ForegroundColor Cyan
    Write-Host ("  PB6 / SCL = " + $(if ($scl) { "HIGH (1)" } else { "LOW (0)" }))
    Write-Host ("  PB7 / SDA = " + $(if ($sda) { "HIGH (1)" } else { "LOW (0)" }))
    Write-Host ""
    if ($scl -eq 1 -and $sda -eq 1) {
        Write-Host "  -> Kedua line idle HIGH. Pull-up bekerja, bus tidak macet." -ForegroundColor Green
        Write-Host "     Tapi OLED tidak ACK => OLED tidak dapat daya, atau SCL/SDA tertukar,"
        Write-Host "     atau modul butuh 5V, atau pin modul urutannya beda."
    } elseif ($scl -eq 0 -and $sda -eq 0) {
        Write-Host "  -> Kedua line LOW. Kemungkinan besar OLED tidak dapat daya" -ForegroundColor Red
        Write-Host "     (VCC/GND tertukar) atau modul menarik line ke ground."
    } else {
        Write-Host "  -> Salah satu line tertahan LOW. Bus macet / kabel bermasalah." -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== Cek MCU masih jalan ===" -ForegroundColor Cyan
$nm = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
$elf = Join-Path $PSScriptRoot "build\diag.elf"
if ($nm -and (Test-Path $elf)) {
    $addr = $null
    & $nm $elf | ForEach-Object {
        $p = $_ -split '\s+'
        if ($p.Count -ge 3 -and $p[2] -eq 'g_loop') { $addr = [Convert]::ToUInt32($p[0],16) }
    }
    if ($addr) {
        $a = Read-Reg $addr
        Start-Sleep -Seconds 2
        $b = Read-Reg $addr
        Write-Host ("  g_loop percobaan 1 = $a")
        Write-Host ("  g_loop percobaan 2 = $b")
        if ($a -eq $b) {
            Write-Host "  -> MCU BERHENTI (hang / hardfault) di luar main loop." -ForegroundColor Red
        } else {
            Write-Host "  -> MCU berjalan normal." -ForegroundColor Green
        }
    }
}
