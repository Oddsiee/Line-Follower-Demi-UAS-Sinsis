# read_oled_state.ps1 — baca counter OLED dari firmware test_sensor yang sedang jalan
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_sensor.elf"
if (-not (Test-Path $elf)) { throw "ELF tidak ada: $elf" }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$nm  = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1).FullName

$syms = @{}
& $nm $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0], 16) }
}

$names = @('g_sh1106_ok','g_sh1106_fail','g_sh1106_last','g_sh1106_recover',
           'g_sh1106_scan_count','g_sh1106_scan_lo','g_sh1106_scan_hi',
           'g_scan_count','g_loop','g_adc_err')

$addrs = @()
foreach ($n in $names) { if ($syms.ContainsKey($n)) { $addrs += $syms[$n] } }
$addrs = $addrs | Sort-Object
$base = $addrs[0]
$end  = $addrs[-1] + 4
$len  = $end - $base

$out = & $cli -c port=SWD mode=hotplug -r32 ("0x{0:X8}" -f $base) ("0x{0:X}" -f $len) 2>&1
$words = @()
foreach ($line in $out) {
    if ($line -match '^(0x[0-9A-Fa-f]{8})\s*:\s*(.+)$') {
        foreach ($v in ($matches[2].Trim() -split '\s+')) {
            if ($v -match '^[0-9A-Fa-f]{8}$') { $words += [Convert]::ToUInt32($v, 16) }
        }
    }
}

Write-Host ""
Write-Host "=== OLED / sensor state (firmware test_sensor) ===" -ForegroundColor Cyan
foreach ($n in $names) {
    if ($syms.ContainsKey($n)) {
        $i = [int](($syms[$n] - $base) / 4)
        Write-Host ("  {0,-20} = {1}" -f $n, $words[$i])
    }
}
