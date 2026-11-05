# read_all.ps1 — Baca semua variabel g_ dalam SATU koneksi SWD
# (setiap '-c' baru akan reset MCU, jadi harus satu koneksi)

param(
    [string]$Elf = "build\pullup.elf",
    [string]$Base = "0x20000028",
    [int]$Words = 8
)

$ErrorActionPreference = 'Stop'
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$nm  = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName

$elfPath = Join-Path $PSScriptRoot $Elf
if (-not (Test-Path $elfPath)) { throw "ELF tidak ada: $elfPath" }

# Peta simbol
$syms = @{}
& $nm $elfPath | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

# Satu koneksi, baca blok memori (parameter kedua = jumlah BYTE)
$bytes = $Words * 4
$out = & $cli -c port=SWD -r32 $Base ("0x{0:X}" -f $bytes) 2>&1

# Kumpulkan semua word dari baris "0xADDR : w0 w1 w2 w3"
$mem = @{}
foreach ($line in $out) {
    if ($line -match '^(0x[0-9A-Fa-f]{8})\s*:\s*(.+)$') {
        $addr = [Convert]::ToUInt32($matches[1], 16)
        $vals = $matches[2].Trim() -split '\s+'
        for ($i = 0; $i -lt $vals.Count; $i++) {
            if ($vals[$i] -match '^[0-9A-Fa-f]{8}$') {
                $mem[$addr + ($i * 4)] = [Convert]::ToUInt32($vals[$i], 16)
            }
        }
    }
}

Write-Host "=== Nilai variabel (satu snapshot) ===" -ForegroundColor Cyan
$syms.GetEnumerator() | Sort-Object Value | ForEach-Object {
    $name = $_.Key
    $addr = $_.Value
    if ($mem.ContainsKey($addr)) {
        $v = $mem[$addr]
        Write-Host ("  {0,-12} @ 0x{1:X8} = 0x{2:X8}  ({2})" -f $name, $addr, $v)
    }
}
