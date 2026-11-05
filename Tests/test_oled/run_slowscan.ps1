# run_slowscan.ps1 — Build + flash + baca hasil scan I2C lambat
$ErrorActionPreference = 'Stop'

$tcRoot = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
if (-not (Test-Path "$tcRoot\arm-none-eabi-gcc.exe")) {
    $found = Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) { throw "ARM GCC tidak ditemukan." }
    $tcRoot = $found.DirectoryName
}
$GCC  = "$tcRoot\arm-none-eabi-gcc.exe"
$SIZE = "$tcRoot\arm-none-eabi-size.exe"
$NM   = "$tcRoot\arm-none-eabi-nm.exe"

$fw = "$env:USERPROFILE\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3"
$here = $PSScriptRoot
$out  = Join-Path $here "build"
New-Item -ItemType Directory -Path $out -Force | Out-Null

$halSrc = "$fw\Drivers\STM32F4xx_HAL_Driver\Src"
$cmsisD = "$fw\Drivers\CMSIS\Device\ST\STM32F4xx"
$cmsisC = "$fw\Drivers\CMSIS\Include"

$cpu = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DSTM32F411xE','-DUSE_HAL_DRIVER')
$inc = @("-I$here","-I$halSrc\..\Inc","-I$cmsisD\Include","-I$cmsisC")
$cflags = $cpu + $inc + @('-std=gnu11','-O2','-g3','-Wall','-ffunction-sections','-fdata-sections')

$sources = @(
    "$here\slow_scan.c",
    "$here\stm32f4xx_it.c",
    "$halSrc\stm32f4xx_hal.c",
    "$halSrc\stm32f4xx_hal_cortex.c",
    "$halSrc\stm32f4xx_hal_dma.c",
    "$halSrc\stm32f4xx_hal_flash.c",
    "$halSrc\stm32f4xx_hal_flash_ex.c",
    "$halSrc\stm32f4xx_hal_gpio.c",
    "$halSrc\stm32f4xx_hal_i2c.c",
    "$halSrc\stm32f4xx_hal_i2c_ex.c",
    "$halSrc\stm32f4xx_hal_pwr.c",
    "$halSrc\stm32f4xx_hal_pwr_ex.c",
    "$halSrc\stm32f4xx_hal_rcc.c",
    "$halSrc\stm32f4xx_hal_rcc_ex.c",
    "$cmsisD\Source\Templates\system_stm32f4xx.c"
)

Write-Host "=== Compile ===" -ForegroundColor Cyan
$objs = @()
foreach ($s in $sources) {
    $o = Join-Path $out ("ss_" + [IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
}
$so = Join-Path $out "ss_startup.o"
& $GCC @cpu -c "$cmsisD\Source\Templates\gcc\startup_stm32f411xe.s" -o $so
$objs += $so
Write-Host "  ok"

Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "slowscan.elf"
if (Test-Path $elf) { Remove-Item $elf -Force }
$ErrorActionPreference = 'Continue'
& $GCC @cpu -T "$here\STM32F411CEUX_FLASH.ld" --specs=nano.specs --specs=nosys.specs '-Wl,--gc-sections' -o $elf @objs -lm 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $elf)) { throw "Gagal link" }
& $SIZE $elf

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"
Start-Sleep -Seconds 3

# --- baca semua variabel dalam SATU koneksi ---
$syms = @{}
& $NM $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

$minAddr = [uint32](($syms.Values | Measure-Object -Minimum).Minimum)
$maxAddr = [uint32](($syms.Values | Measure-Object -Maximum).Maximum)
$base    = $minAddr
$bytes   = [int](($maxAddr - $minAddr) + 4)

$out2 = & $cli -c port=SWD -r32 ("0x{0:X8}" -f $base) ("0x{0:X}" -f $bytes) 2>&1

$mem = @{}
foreach ($line in $out2) {
    if ($line -match '^(0x[0-9A-Fa-f]{8})\s*:\s*(.+)$') {
        $a = [Convert]::ToUInt32($matches[1], 16)
        $vals = $matches[2].Trim() -split '\s+'
        for ($i = 0; $i -lt $vals.Count; $i++) {
            if ($vals[$i] -match '^[0-9A-Fa-f]{8}$') { $mem[$a + ($i*4)] = [Convert]::ToUInt32($vals[$i],16) }
        }
    }
}

function V($n) { if ($syms.ContainsKey($n) -and $mem.ContainsKey($syms[$n])) { return $mem[$syms[$n]] } else { return $null } }

Write-Host ""
Write-Host "=== Hasil scan I2C lambat (pull-up internal, ~5 kHz) ===" -ForegroundColor Cyan
$idle = V 'g_idle_state'
$puok = V 'g_pullup_ok'
$ack3c = V 'g_ack_3c'
$ack3d = V 'g_ack_3d'
$found = V 'g_found'
$done  = V 'g_done'

Write-Host ("  g_idle_state  = 0x{0:X2}   (0xC0 = SCL+SDA HIGH)" -f $idle)
Write-Host ("  g_pullup_ok   = {0}" -f $puok)
Write-Host ("  g_ack_3c      = {0}" -f $ack3c)
Write-Host ("  g_ack_3d      = {0}" -f $ack3d)
Write-Host ("  g_found       = 0x{0:X2}" -f $found)
Write-Host ("  g_done        = {0}" -f $done)

Write-Host ""
Write-Host "=== Kesimpulan ===" -ForegroundColor Cyan
if ($found -eq 0x3C -or $found -eq 0x3D) {
    Write-Host ("  [OK] OLED HIDUP di alamat 0x{0:X2}!" -f $found) -ForegroundColor Green
    Write-Host "       Modul sehat. Masalahnya cuma TIDAK ADA PULL-UP di bus."
    Write-Host ""
    Write-Host "  SOLUSI: tambah resistor 4.7k dari SCL->3V3 dan SDA->3V3," -ForegroundColor Yellow
    Write-Host "          ATAU pakai pull-up internal MCU (sudah terbukti jalan)."
} elseif ($puok -eq 1) {
    Write-Host "  [!!] Pull-up internal bekerja (line bisa HIGH)," -ForegroundColor Red
    Write-Host "       tapi OLED tetap tidak ACK."
    Write-Host "       => Modul tidak tersambung ke PB6/PB7, atau modul rusak."
} else {
    Write-Host "  [!!] Bahkan pull-up internal tidak bisa membuat line HIGH." -ForegroundColor Red
    Write-Host "       => Ada yang menahan SCL/SDA ke GND (short), atau pin rusak."
}
