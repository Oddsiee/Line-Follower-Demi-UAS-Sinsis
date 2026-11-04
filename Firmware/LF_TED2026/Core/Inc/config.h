#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ============================================================
 * config.h — semua konstanta proyek di satu tempat.
 * Pin: gunakan User Label CubeMX (MUX_IN1_Pin, BTN_UP_Pin, dst.).
 * ============================================================ */

/* ---- Sensor Dariyan-X 16CH ---- */
#define SENSOR_COUNT        16
#define SENSOR_GROUPS       4       /* Out1..Out4 -> ADC PA4..PA7        */
#define MUX_STATES          4       /* (In2,In1) = 00,01,10,11           */
#define MUX_SETTLE_US       10      /* jeda settling setelah ganti mux   */
#define ADC_MAX             4095
#define SENSOR_NORM_MAX     1000    /* nilai hasil normalisasi           */
#define SENSOR_POS_MAX      7500    /* posisi: -7500..+7500, 0 = tengah  */
#define SENSOR_POS_SCALE    1000    /* (indeks - 7.5) * 1000             */
#define SENSOR_ON_LINE_MIN  300     /* ambang norm: dianggap "melihat garis" */

/* ---- Motor (ZK-5AD, TIM1) ---- */
#define MOTOR_CMD_MAX       1000    /* Motor_Set: -1000..+1000           */
#define MOTOR_PWM_ARR       999     /* ARR TIM1 = MOTOR_CMD_MAX - 1      */

/* ---- Loop kontrol ---- */
#define LOOP_PERIOD_MS      5       /* titik awal; tentukan di M8.1      */

/* ---- Default parameter (TITIK AWAL, bukan hasil tuning) ---- */
#define DEFAULT_KP          0.05f
#define DEFAULT_KI          0.0f
#define DEFAULT_KD          0.50f
#define DEFAULT_BASE_SPEED  400
#define DEFAULT_MAX_SPEED   800

/* ---- Penyimpanan Flash (STM32F411, sektor 7 = 128 KB terakhir) ---- */
#define FLASH_PARAMS_ADDR   0x08060000UL
#define PARAMS_MAGIC        0x4C465432UL   /* "LFT2" */

/* ---- Batas regulasi TED 2026 (untuk dokumentasi/cek) ---- */
#define ROBOT_MAX_L_MM      200
#define ROBOT_MAX_W_MM      200
#define ROBOT_MAX_H_MM      100
#define ROBOT_MAX_MASS_G    3000
#define BATT_NOMINAL_MV     12000
#define BATT_TOL_MV         1500

#endif /* CONFIG_H */
