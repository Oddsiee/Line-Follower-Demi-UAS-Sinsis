# run_pullup.ps1 — Build + flash + baca hasil tes pull-up
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
    "$here\pullup_test.c",
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
    $o = Join-Path $out ("pu_" + [IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    & $GCC @cflags -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "Gagal compile: $s" }
    $objs += $o
}
$so = Join-Path $out "pu_startup.o"
& $GCC @cpu -c "$cmsisD\Source\Templates\gcc\startup_stm32f411xe.s" -o $so
$objs += $so
Write-Host "  ok"

Write-Host "=== Link ===" -ForegroundColor Cyan
$elf = Join-Path $out "pullup.elf"
if (Test-Path $elf) { Remove-Item $elf -Force }
$ErrorActionPreference = 'Continue'
& $GCC @cpu -T "$here\STM32F411CEUX_FLASH.ld" --specs=nano.specs --specs=nosys.specs '-Wl,--gc-sections' -o $elf @objs -lm 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $elf)) { throw "Gagal link" }
& $SIZE $elf

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

Write-Host "=== Flash ===" -ForegroundColor Cyan
& $cli -c port=SWD -w $elf -v -rst 2>&1 | Select-String -Pattern "Download verified|Error|MCU Reset"
Start-Sleep -Seconds 2

$syms = @{}
& $NM $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') { $syms[$p[2]] = [Convert]::ToUInt32($p[0],16) }
}

function Read-Sym($name) {
    if (-not $syms.ContainsKey($name)) { return $null }
    $a = "0x{0:X8}" -f $syms[$name]
    $o = & $cli -c port=SWD -r32 $a 0x1 2>&1 | Select-String -Pattern "^0x"
    if (-not $o) { return $null }
    $line = @($o)[-1].Line
    return ($line -split '\s+')[-1]
}

Write-Host ""
Write-Host "=== Hasil tes pull-up ===" -ForegroundColor Cyan
$pd6 = Read-Sym 'g_pd_pb6'
$pd7 = Read-Sym 'g_pd_pb7'
$pdb = Read-Sym 'g_pd_both'
$pub = Read-Sym 'g_pu_both'
$flt = Read-Sym 'g_float'

function B($h, $bit) {
    if (-not $h) { return "?" }
    $v = [Convert]::ToUInt32($h,16)
    return (($v -shr $bit) -band 1)
}

Write-Host ("  PB6 pull-down  : $pd6  -> PB6=" + (B $pd6 6))
Write-Host ("  PB7 pull-down  : $pd7  -> PB7=" + (B $pd7 7))
Write-Host ("  both pull-down : $pdb  -> PB6=" + (B $pdb 6) + " PB7=" + (B $pdb 7))
Write-Host ("  both pull-up   : $pub  -> PB6=" + (B $pub 6) + " PB7=" + (B $pub 7))
Write-Host ("  both float     : $flt  -> PB6=" + (B $flt 6) + " PB7=" + (B $flt 7))

Write-Host ""
Write-Host "=== Kesimpulan ===" -ForegroundColor Cyan
$s6 = B $pdb 6
$s7 = B $pdb 7

if ($s6 -eq 1 -and $s7 -eq 1) {
    Write-Host "  [OK] Dengan pull-down internal, SCL & SDA tetap HIGH." -ForegroundColor Green
    Write-Host "       => Modul OLED TERHUBUNG dan BERDAYA (pull-up 10k di modul menang)."
    Write-Host "       => Masalahnya di protokol/init, bukan kabel. Kabari aku."
} elseif ($s6 -eq 0 -and $s7 -eq 0) {
    Write-Host "  [!!] Dengan pull-down, SCL & SDA = LOW." -ForegroundColor Red
    Write-Host "       => Modul OLED TIDAK terhubung, atau TIDAK mendapat daya."
    Write-Host ""
    Write-Host "  Yang harus dicek (urut kemungkinan):" -ForegroundColor Yellow
    Write-Host "   1. VCC OLED benar-benar dapat 3.3V? Ukur dengan multimeter."
    Write-Host "   2. GND OLED tersambung ke GND board?"
    Write-Host "   3. Kabel SCL/SDA benar-benar masuk ke PB6/PB7 (bukan pin sebelah)?"
    Write-Host "   4. Urutan pin modul: VCC-GND-SCL-SDA (bisa terbalik di modul lain!)"
    Write-Host "   5. Coba modul OLED lain kalau ada."
} else {
    Write-Host "  [??] Hanya satu line yang HIGH." -ForegroundColor Yellow
    Write-Host "       => Satu kabel tersambung, satu tidak. Cek kabel yang LOW."
}
