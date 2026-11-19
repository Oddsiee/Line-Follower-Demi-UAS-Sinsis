# build.ps1 — Build firmware Line Follower Coding Utama (STM32F401CCU6)
#
# Pakai:  .\build.ps1
# Hasil:  build\Line_Follower_Coding_Utama.elf (.hex, .bin)

$ErrorActionPreference = 'Stop'

# ---------- Lokasi toolchain ----------
$tcRoot = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
if (-not (Test-Path "$tcRoot\arm-none-eabi-gcc.exe")) {
    $found = Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $found) { throw "ARM GCC tidak ditemukan. Install STM32CubeIDE dulu." }
    $tcRoot = $found.DirectoryName
}
$GCC     = "$tcRoot\arm-none-eabi-gcc.exe"
$OBJCOPY = "$tcRoot\arm-none-eabi-objcopy.exe"
$SIZE    = "$tcRoot\arm-none-eabi-size.exe"

# ---------- Path proyek lokal (STM32F401CCU6) ----------
$here = $PSScriptRoot
$out  = Join-Path $here "build"
New-Item -ItemType Directory -Path $out -Force | Out-Null

$coreInc = Join-Path $here "Core\Inc"
$coreSrc = Join-Path $here "Core\Src"
$startup = Join-Path $here "Core\Startup\startup_stm32f401ccux.s"
$ld      = Join-Path $here "STM32F401CCUX_FLASH.ld"
$halInc  = Join-Path $here "Drivers\STM32F4xx_HAL_Driver\Inc"
$halLeg  = Join-Path $here "Drivers\STM32F4xx_HAL_Driver\Inc\Legacy"
$halSrc  = Join-Path $here "Drivers\STM32F4xx_HAL_Driver\Src"
$cmsisD  = Join-Path $here "Drivers\CMSIS\Device\ST\STM32F4xx\Include"
$cmsisC  = Join-Path $here "Drivers\CMSIS\Include"

if (-not (Test-Path $startup)) { throw "Startup file tidak ada: $startup" }
if (-not (Test-Path $ld))      { throw "Linker script tidak ada: $ld" }

# ---------- Flags ----------
$cpu = @(
    '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard',
    '-DSTM32F401xC', '-DUSE_HAL_DRIVER'
)
$inc = @(
    "-I$coreInc",
    "-I$halInc",
    "-I$halLeg",
    "-I$cmsisD",
    "-I$cmsisC"
)
$cflags = $cpu + $inc + @('-std=gnu11', '-O2', '-g3', '-Wall', '-ffunction-sections', '-fdata-sections')

# ---------- Daftar file sumber ----------
$sources = @()
$sources += Get-ChildItem -Path $coreSrc -Filter "*.c" | Sort-Object Name | Select-Object -ExpandProperty FullName
$sources += Get-ChildItem -Path $halSrc  -Filter "*.c" | Sort-Object Name | Select-Object -ExpandProperty FullName

Write-Host "=== Compile (STM32F401CCU6) ===" -ForegroundColor Cyan
$objs = @()
foreach ($s in $sources) {
    $o = Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
    Write-Host ("  ok  " + [IO.Path]::GetFileName($s))
}

$so = Join-Path $out "startup_stm32f401ccux.o"
& $GCC @cpu -c $startup -o $so
if ($LASTEXITCODE -ne 0) { throw "Gagal compile startup: $startup" }
$objs += $so
Write-Host "  ok  startup_stm32f401ccux.s"

# ---------- Link ----------
Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "Line_Follower_Coding_Utama.elf"
$map = Join-Path $out "Line_Follower_Coding_Utama.map"
if (Test-Path $elf) { Remove-Item $elf -Force }

& $GCC @cpu -T $ld --specs=nano.specs `
    '-Wl,--gc-sections' "-Wl,-Map=$map" -o $elf @objs -lm
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $elf)) {
    throw "Gagal link (Line_Follower_Coding_Utama.elf tidak terbentuk)"
}

# ---------- Output ----------
Write-Host "=== Hasil ===" -ForegroundColor Green
& $SIZE $elf

$hex = Join-Path $out "Line_Follower_Coding_Utama.hex"
$bin = Join-Path $out "Line_Follower_Coding_Utama.bin"
& $OBJCOPY -O ihex   $elf $hex
& $OBJCOPY -O binary $elf $bin

Write-Host ""
Write-Host "ELF : $elf"
Write-Host "HEX : $hex"
Write-Host "BIN : $bin"
Write-Host ""
Write-Host "Flash dengan:  .\flash.ps1" -ForegroundColor Yellow
