# build_diag.ps1 — Build firmware diagnostik OLED
#
# Pakai:  .\build_diag.ps1
# Hasil:  build\diag.elf

$ErrorActionPreference = 'Stop'

$tcRoot = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
if (-not (Test-Path "$tcRoot\arm-none-eabi-gcc.exe")) {
    $found = Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $found) { throw "ARM GCC tidak ditemukan." }
    $tcRoot = $found.DirectoryName
}
$GCC  = "$tcRoot\arm-none-eabi-gcc.exe"
$SIZE = "$tcRoot\arm-none-eabi-size.exe"

$fw = "$env:USERPROFILE\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3"
if (-not (Test-Path "$fw\Drivers\STM32F4xx_HAL_Driver\Inc\stm32f4xx_hal.h")) {
    throw "Firmware package tidak ditemukan di $fw"
}

$here = $PSScriptRoot
$out  = Join-Path $here "build"
New-Item -ItemType Directory -Path $out -Force | Out-Null

$halSrc = "$fw\Drivers\STM32F4xx_HAL_Driver\Src"
$cmsisD = "$fw\Drivers\CMSIS\Device\ST\STM32F4xx"
$cmsisC = "$fw\Drivers\CMSIS\Include"

$cpu = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard',
         '-DSTM32F411xE','-DUSE_HAL_DRIVER')
$inc = @("-I$here","-I$halSrc\..\Inc","-I$cmsisD\Include","-I$cmsisC")
$cflags = $cpu + $inc + @('-std=gnu11','-O2','-g3','-Wall','-ffunction-sections','-fdata-sections')

$sources = @(
    "$here\diag_main.c",
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
    if (-not (Test-Path $s)) { throw "Sumber tidak ada: $s" }
    $o = Join-Path $out ("diag_" + [IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
    Write-Host ("  ok  " + [IO.Path]::GetFileName($s))
}

$so = Join-Path $out "diag_startup.o"
& $GCC @cpu -c "$cmsisD\Source\Templates\gcc\startup_stm32f411xe.s" -o $so
if ($LASTEXITCODE -ne 0) { throw "Gagal compile startup" }
$objs += $so
Write-Host "  ok  startup_stm32f411xe.s"

Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "diag.elf"
if (Test-Path $elf) { Remove-Item $elf -Force }
$ErrorActionPreference = 'Continue'
& $GCC @cpu -T "$here\STM32F411CEUX_FLASH.ld" --specs=nano.specs --specs=nosys.specs `
    '-Wl,--gc-sections' -o $elf @objs -lm 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $elf)) { throw "Gagal link (diag.elf tidak terbentuk)" }

Write-Host "=== Hasil ===" -ForegroundColor Green
& $SIZE $elf
Write-Host ""
Write-Host "ELF : $elf"
Write-Host "Flash dengan:  .\flash_diag.ps1" -ForegroundColor Yellow
