# build.ps1 — Build firmware test OVERALL (OLED + Tombol + Sensor + Motor)
#
# Pakai:  .\build.ps1
# Hasil:  build\test_overall.elf  (siap di-flash)
#
# Butuh: ARM GCC bawaan STM32CubeIDE + firmware package STM32Cube_FW_F4_V1.28.3

$ErrorActionPreference = 'Stop'

# ---------- Lokasi toolchain ----------
$tcRoot = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
if (-not (Test-Path "$tcRoot\arm-none-eabi-gcc.exe")) {
    $found = Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $found) { throw "ARM GCC tidak ditemukan. Install STM32CubeIDE dulu." }
    $tcRoot = $found.DirectoryName
}
$GCC  = "$tcRoot\arm-none-eabi-gcc.exe"
$SIZE = "$tcRoot\arm-none-eabi-size.exe"

# ---------- Firmware package ----------
$fw = "$env:USERPROFILE\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3"
if (-not (Test-Path "$fw\Drivers\STM32F4xx_HAL_Driver\Inc\stm32f4xx_hal.h")) {
    throw "Firmware package tidak ditemukan di $fw."
}

# ---------- Path ----------
$here = $PSScriptRoot
$repo = Join-Path $here "..\..\Firmware\LF_TED2026\Core"
$out  = Join-Path $here "build"
New-Item -ItemType Directory -Path $out -Force | Out-Null

$halSrc = "$fw\Drivers\STM32F4xx_HAL_Driver\Src"
$cmsisD = "$fw\Drivers\CMSIS\Device\ST\STM32F4xx"
$cmsisC = "$fw\Drivers\CMSIS\Include"

# ---------- Flags ----------
$cpu = @(
    '-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard',
    '-DSTM32F411xE','-DUSE_HAL_DRIVER'
)
$inc = @(
    "-I$here",
    "-I$repo\Inc",
    "-I$halSrc\..\Inc",
    "-I$cmsisD\Include",
    "-I$cmsisC"
)
$cflags = $cpu + $inc + @('-std=gnu11','-O2','-g3','-Wall','-ffunction-sections','-fdata-sections')

# ---------- Sumber ----------
# sh1106.c, motor.c, & buttons.c diambil langsung dari Core proyek.
$sources = @(
    "$here\main.c",
    "$here\stm32f4xx_it.c",
    "$repo\Src\sh1106.c",
    "$repo\Src\motor.c",
    "$repo\Src\buttons.c",
    "$halSrc\stm32f4xx_hal.c",
    "$halSrc\stm32f4xx_hal_cortex.c",
    "$halSrc\stm32f4xx_hal_dma.c",
    "$halSrc\stm32f4xx_hal_flash.c",
    "$halSrc\stm32f4xx_hal_flash_ex.c",
    "$halSrc\stm32f4xx_hal_gpio.c",
    "$halSrc\stm32f4xx_hal_adc.c",
    "$halSrc\stm32f4xx_hal_adc_ex.c",
    "$halSrc\stm32f4xx_hal_i2c.c",
    "$halSrc\stm32f4xx_hal_i2c_ex.c",
    "$halSrc\stm32f4xx_hal_tim.c",
    "$halSrc\stm32f4xx_hal_tim_ex.c",
    "$halSrc\stm32f4xx_hal_pwr.c",
    "$halSrc\stm32f4xx_hal_pwr_ex.c",
    "$halSrc\stm32f4xx_hal_rcc.c",
    "$halSrc\stm32f4xx_hal_rcc_ex.c",
    "$cmsisD\Source\Templates\system_stm32f4xx.c"
)

Write-Host "=== Compile ===" -ForegroundColor Cyan
$objs = @()
foreach ($s in $sources) {
    if (-not (Test-Path $s)) { throw "Sumber tidak ada: $s" }
    $o = Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
    Write-Host ("  ok  " + [IO.Path]::GetFileName($s))
}

# startup assembly
$startup = "$cmsisD\Source\Templates\gcc\startup_stm32f411xe.s"
$so = Join-Path $out "startup_stm32f411xe.o"
& $GCC @cpu -c $startup -o $so
if ($LASTEXITCODE -ne 0) { throw "Gagal compile startup" }
$objs += $so
Write-Host "  ok  startup_stm32f411xe.s"

# ---------- Link ----------
Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "test_overall.elf"
if (Test-Path $elf) { Remove-Item $elf -Force }
$ErrorActionPreference = 'Continue'
& $GCC @cpu -T "$here\STM32F411CEUX_FLASH.ld" --specs=nano.specs --specs=nosys.specs `
    '-Wl,--gc-sections' '-Wl,-Map=build\test_overall.map' -o $elf @objs -lm 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $elf)) { throw "Gagal link (test_overall.elf tidak terbentuk)" }

# ---------- Output ----------
Write-Host "=== Hasil ===" -ForegroundColor Green
& $SIZE $elf

$hex = Join-Path $out "test_overall.hex"
$bin = Join-Path $out "test_overall.bin"
& "$tcRoot\arm-none-eabi-objcopy.exe" -O ihex   $elf $hex
& "$tcRoot\arm-none-eabi-objcopy.exe" -O binary $elf $bin

Write-Host ""
Write-Host "ELF : $elf"
Write-Host "HEX : $hex"
Write-Host "BIN : $bin"
Write-Host ""
Write-Host "Flash dengan:  .\flash.ps1" -ForegroundColor Yellow
