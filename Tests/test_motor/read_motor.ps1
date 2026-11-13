# read_motor.ps1 — Baca status uji driver motor via SWD (tanpa UART, tanpa reset)
#
# Pakai:  .\read_motor.ps1 [-Samples 5] [-IntervalMs 500]
# Butuh: build\test_motor.elf (jalankan .\build.ps1 dulu) + board ter-flash
#
# Yang dibaca:
#   g_phase, g_phase_count, g_loop, g_cmd_left, g_cmd_right     (variabel firmware)
#   g_oled_ok, g_paused, g_btn_ok, g_btn_back, g_moe_on
#   Register TIM1: CR1, CCER, CNT, CCR1..CCR4, BDTR + RCC_APB2ENR + GPIOA MODER/AFRH
#
# Mode hotplug => MCU TIDAK di-reset, jadi nilai bergerak terlihat live.

param(
    [int]$Samples = 1,
    [int]$IntervalMs = 500
)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$elf  = Join-Path $here "build\test_motor.elf"
if (-not (Test-Path $elf)) { throw "ELF tidak ada: $elf. Jalankan .\build.ps1 dulu." }

$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path $cli)) { throw "STM32_Programmer_CLI.exe tidak ditemukan." }

$nm = (Get-ChildItem "C:\ST" -Recurse -Filter "arm-none-eabi-nm.exe" -ErrorAction SilentlyContinue |
       Select-Object -First 1).FullName
if (-not $nm) { throw "arm-none-eabi-nm.exe tidak ditemukan." }

# ---------- Peta simbol ----------
$syms = @{}
& $nm $elf | ForEach-Object {
    $p = $_ -split '\s+'
    if ($p.Count -ge 3 -and $p[2] -match '^g_') {
        $syms[$p[2]] = [Convert]::ToUInt32($p[0], 16)
    }
}

function Read-Block([uint32]$addr, [int]$bytes) {
    $out = & $cli -c port=SWD mode=hotplug -r32 ("0x{0:X8}" -f $addr) ("0x{0:X}" -f $bytes) 2>&1
    $words = @()
    foreach ($line in $out) {
        if ($line -match '^(0x[0-9A-Fa-f]{8})\s*:\s*(.+)$') {
            foreach ($v in ($matches[2].Trim() -split '\s+')) {
                if ($v -match '^[0-9A-Fa-f]{8}$') { $words += [Convert]::ToUInt32($v, 16) }
            }
        }
    }
    return $words
}

function Read-U32([string]$name) {
    if (-not $syms.ContainsKey($name)) { return $null }
    $w = Read-Block $syms[$name] 4
    if ($w.Count -eq 0) { return $null }
    return $w[0]
}

function Read-I16([string]$name) {
    $w = Read-U32 $name
    if ($null -eq $w) { return $null }
    return [int16]([int32]($w -band 0xFFFF))
}

# ---------- Cek koneksi SWD dulu ----------
$probe = & $cli -l 2>&1
if ($probe -match 'DEV_CONNECT_ERR' -or -not ($probe -match 'ST-LINK FW\s*:\s*\S')) {
    Write-Host ""
    Write-Host "ST-Link tidak bisa diakses (DEV_CONNECT_ERR / FW kosong)." -ForegroundColor Red
    Write-Host ""
    Write-Host "Coba langkah ini:" -ForegroundColor Yellow
    Write-Host "  1. Cabut ST-Link dari USB, tunggu 3 detik, colok lagi."
    Write-Host "  2. Tutup STM32CubeIDE / STM32CubeProgrammer kalau sedang terbuka."
    Write-Host "  3. Pastikan GND ST-Link tersambung ke GND board."
    Write-Host "  4. Kalau board diberi power eksternal, GND-nya WAJIB disambung ke GND ST-Link."
    Write-Host ""
    exit 1
}

# ---------- Alamat register (STM32F411) ----------
$TIM1_BASE  = 0x40010000
$RCC_APB2ENR = 0x40023844
$GPIOA_MODER = 0x40020000
$GPIOA_AFRH  = 0x40020024

for ($n = 0; $n -lt $Samples; $n++) {
    if ($Samples -gt 1) { Write-Host ("--- sampel {0}/{1} ---" -f ($n + 1), $Samples) -ForegroundColor DarkGray }

    $phase = Read-U32 'g_phase'
    $pcount = Read-U32 'g_phase_count'
    $loop  = Read-U32 'g_loop'
    $cl    = Read-I16 'g_cmd_left'
    $cr    = Read-I16 'g_cmd_right'
    $oled  = Read-U32 'g_oled_ok'
    $pause = Read-U32 'g_paused'
    $bok   = Read-U32 'g_btn_ok'
    $bback = Read-U32 'g_btn_back'
    $moe   = Read-U32 'g_moe_on'

    if ($null -eq $loop) {
        Write-Host ""
        Write-Host "Gagal membaca variabel g_* dari MCU." -ForegroundColor Red
        Write-Host "Pastikan board sudah di-flash firmware test_motor dan ST-Link stabil." -ForegroundColor Yellow
        exit 1
    }

    # TIM1: baca CR1..BDTR sekaligus (offset 0x00..0x44 = 72 byte)
    $t = Read-Block $TIM1_BASE 72
    $apb2 = Read-Block $RCC_APB2ENR 4
    $moder = Read-Block $GPIOA_MODER 4
    $afrh  = Read-Block $GPIOA_AFRH 4

    Write-Host ""
    Write-Host "=== Firmware ===" -ForegroundColor Cyan
    Write-Host ("  fase        : {0}/{1}   ({2})" -f $phase, $pcount, ($(if ($pause) { 'PAUSE' } else { 'AUTO' })))
    Write-Host ("  loop        : {0}" -f $loop)
    Write-Host ("  cmd kiri    : {0}" -f $cl)
    Write-Host ("  cmd kanan   : {0}" -f $cr)
    Write-Host ("  OLED ok     : {0}" -f $oled)
    Write-Host ("  TIM1 MOE    : {0}" -f $moe)
    Write-Host ("  btn OK/BACK : {0}/{1}" -f $bok, $bback)

    if ($t.Count -ge 18) {
        $cr1  = $t[0]
        $ccer = $t[8]      # 0x20 / 4
        $cnt  = $t[9]      # 0x24 / 4
        $ccr1 = $t[13]     # 0x34 / 4
        $ccr2 = $t[14]
        $ccr3 = $t[15]
        $ccr4 = $t[16]
        $bdtr = $t[17]     # 0x44 / 4

        Write-Host ""
        Write-Host "=== Register TIM1 ===" -ForegroundColor Cyan
        Write-Host ("  CR1   = 0x{0:X8}   (CEN={1})" -f $cr1, (($cr1 -shr 0) -band 1))
        Write-Host ("  CCER  = 0x{0:X8}   (CC1E..CC4E = {1}{2}{3}{4})" -f $ccer,
                    (($ccer -shr 0) -band 1), (($ccer -shr 4) -band 1),
                    (($ccer -shr 8) -band 1), (($ccer -shr 12) -band 1))
        Write-Host ("  CNT   = {0}" -f $cnt)
        Write-Host ("  CCR1 (D0/PA8 ) = {0}" -f $ccr1)
        Write-Host ("  CCR2 (D1/PA9 ) = {0}" -f $ccr2)
        Write-Host ("  CCR3 (D2/PA10) = {0}" -f $ccr3)
        Write-Host ("  CCR4 (D3/PA11) = {0}" -f $ccr4)
        Write-Host ("  BDTR  = 0x{0:X8}   (MOE={1})" -f $bdtr, (($bdtr -shr 15) -band 1))

        $cen = ($cr1 -band 1)
        if ($cen -eq 0) {
            Write-Host "  PERINGATAN: TIM1 CEN=0 -> counter belum jalan, PWM tidak keluar." -ForegroundColor Yellow
        }
        if ((($bdtr -shr 15) -band 1) -eq 0) {
            Write-Host "  PERINGATAN: BDTR.MOE=0 -> output TIM1 tidak aktif di pin." -ForegroundColor Yellow
        }
    }

    if ($apb2.Count -ge 1) {
        Write-Host ""
        Write-Host "=== Peripheral / GPIO ===" -ForegroundColor Cyan
        Write-Host ("  RCC_APB2ENR   = 0x{0:X8}  (TIM1EN={1})" -f $apb2[0], ($apb2[0] -band 1))
    }
    if ($moder.Count -ge 1) {
        $m = $moder[0]
        $af8  = ($m -shr 16) -band 3
        $af9  = ($m -shr 18) -band 3
        $af10 = ($m -shr 20) -band 3
        $af11 = ($m -shr 22) -band 3
        Write-Host ("  GPIOA_MODER   = 0x{0:X8}  (PA8..PA11 mode = {1} {2} {3} {4}, harus 10=AF)" -f $m, $af8, $af9, $af10, $af11)
    }
    if ($afrh.Count -ge 1) {
        $a = $afrh[0]
        Write-Host ("  GPIOA_AFRH    = 0x{0:X8}  (PA8..PA11 AF = {1} {2} {3} {4}, harus 1=TIM1)" -f $a,
                    (($a -shr 0) -band 0xF), (($a -shr 4) -band 0xF),
                    (($a -shr 8) -band 0xF), (($a -shr 12) -band 0xF))
    }

    if ($n -lt $Samples - 1) { Start-Sleep -Milliseconds $IntervalMs }
}
