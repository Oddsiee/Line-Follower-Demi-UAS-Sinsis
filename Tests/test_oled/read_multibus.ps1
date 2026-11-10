# read_multibus.ps1 — Flash multibus + baca hasil scan semua bus
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\multibus.elf"
if (-not (Test-Path $elf)) { throw "Jalankan .\build_multibus.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$nm  = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"

Start-Sleep -Seconds 4

$syms = @{}
& $nm $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

function Read-Mem($addr, $count) {
    $a = "0x{0:X8}" -f $addr
    $out = & $cli -c port=SWD -r32 $a ("0x{0:X}" -f $count) 2>&1 | Select-String -Pattern "^0x"
    return $out
}

Write-Host ""
Write-Host "=== Hasil scan multi-bus ===" -ForegroundColor Cyan

$names = @(
  "A. bit-bang PB6/PB7",
  "B. I2C1 PB6/PB7 (AF4)",
  "C. I2C1 PB8/PB9 (AF4)",
  "D. I2C2 PB10/PB11 (AF4)",
    "E. I2C3 PA8/PC9 (AF4)",
    "F. bit-bang SCL/SDA TERTUKAR"
)

if ($syms.ContainsKey('g_bus_count')) {
    $base = $syms['g_bus_count']
    $lines = Read-Mem $base 6
    $vals = @()
    foreach ($l in $lines) {
        $parts = $l -split '\s+'
        for ($i = 2; $i -lt $parts.Count; $i++) { if ($parts[$i] -match '^[0-9A-Fa-f]{8}$') { $vals += [Convert]::ToUInt32($parts[$i],16) } }
    }
    for ($i = 0; $i -lt 6; $i++) {
        $c = if ($i -lt $vals.Count) { $vals[$i] } else { 0 }
        $mark = if ($c -gt 0) { "  <== ADA DEVICE" } else { "" }
        Write-Host ("  {0,-26} : {1} device{2}" -f $names[$i], $c, $mark)
    }
}

Write-Host ""
if ($syms.ContainsKey('g_bus')) {
    $base = $syms['g_bus']
    $lines = Read-Mem $base 5
    $vals = @()
    foreach ($l in $lines) {
        $parts = $l -split '\s+'
        for ($i = 2; $i -lt $parts.Count; $i++) { if ($parts[$i] -match '^[0-9A-Fa-f]{8}$') { $vals += [Convert]::ToUInt32($parts[$i],16) } }
    }
    Write-Host "=== Alamat yang ACK (packed, 1 byte per alamat) ===" -ForegroundColor Cyan
    for ($i = 0; $i -lt 5; $i++) {
        $v = if ($i -lt $vals.Count) { $vals[$i] } else { 0 }
        if ($v -eq 0) { continue }
        $addrs = @()
        for ($b = 3; $b -ge 0; $b--) {
            $byte = ($v -shr ($b*8)) -band 0xFF
            if ($byte -ne 0) { $addrs += ("0x{0:X2}" -f $byte) }
        }
        Write-Host ("  {0,-26} : {1}" -f $names[$i], ($addrs -join ", "))
    }
}

Write-Host ""
Write-Host "=== Kesimpulan ===" -ForegroundColor Cyan
Write-Host "  Kalau SEMUA bus = 0 device:"
Write-Host "    -> OLED tidak mendapat daya, atau kabel tidak tersambung ke pin mana pun,"
Write-Host "       atau modul OLED rusak."
Write-Host "  Kalau ada bus yang menemukan device:"
Write-Host "    -> OLED terpasang di pin tersebut, bukan PB6/PB7."
