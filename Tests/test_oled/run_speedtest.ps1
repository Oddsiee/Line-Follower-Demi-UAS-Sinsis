# run_speedtest.ps1 — Cari kecepatan I2C yang bekerja + tampilkan layar putih
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
    "$here\speed_test.c",
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
    $o = Join-Path $out ("sp_" + [IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
}
$so = Join-Path $out "sp_startup.o"
& $GCC @cpu -c "$cmsisD\Source\Templates\gcc\startup_stm32f411xe.s" -o $so
$objs += $so
Write-Host "  ok"

Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "speedtest.elf"
if (Test-Path $elf) { Remove-Item $elf -Force }
$ErrorActionPreference = 'Continue'
& $GCC @cpu -T "$here\STM32F411CEUX_FLASH.ld" --specs=nano.specs --specs=nosys.specs '-Wl,--gc-sections' -o $elf @objs -lm 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $elf)) { throw "Gagal link" }
& $SIZE $elf

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"
Start-Sleep -Seconds 4

# --- baca dalam satu koneksi, TANPA reset (hotplug) ---
$syms = @{}
& $NM $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

$minAddr = [uint32](($syms.Values | Measure-Object -Minimum).Minimum)
$maxAddr = [uint32](($syms.Values | Measure-Object -Maximum).Maximum)
$bytes   = [int](($maxAddr - $minAddr) + 4)

$out2 = & $cli -c port=SWD mode=hotplug -r32 ("0x{0:X8}" -f $minAddr) ("0x{0:X}" -f $bytes) 2>&1

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
Write-Host "=== Hasil uji kecepatan I2C ===" -ForegroundColor Cyan
$speeds = @(400000,200000,100000,50000,20000,10000)
for ($i = 0; $i -lt 6; $i++) {
    $a = V ("g_ack")   # array base
    $addr = $syms['g_ack'] + ($i * 4)
    $v = if ($mem.ContainsKey($addr)) { $mem[$addr] } else { 0 }
    $mark = if ($v -ne 0) { "  <== ACK 0x{0:X2}" -f $v } else { "" }
    Write-Host ("  {0,7} Hz : {1}{2}" -f $speeds[$i], $(if ($v) { "OK" } else { "--" }), $mark)
}

$best  = V 'g_best_speed'
$baddr = V 'g_best_addr'
$init  = V 'g_init_ok'
$white = V 'g_white_ok'

Write-Host ""
Write-Host ("  kecepatan terbaik : {0} Hz" -f $best)
Write-Host ("  alamat            : 0x{0:X2}" -f $baddr)
Write-Host ("  init SH1106       : {0}" -f $init)
Write-Host ("  layar putih       : {0}" -f $white)

Write-Host ""
if ($white -eq 1) {
    Write-Host "  [OK] LAYAR HARUSNYA PUTIH SEKARANG!" -ForegroundColor Green
    Write-Host "       Kalau putih => OLED sehat, cuma butuh pull-up."
} elseif ($best -ne 0) {
    Write-Host "  [--] OLED ACK tapi tulis data gagal." -ForegroundColor Yellow
} else {
    Write-Host "  [!!] Tidak ada kecepatan yang berhasil." -ForegroundColor Red
}
