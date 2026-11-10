#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ============================================================
 * config.h — semua konstanta proyek di satu tempat.
 * Pin: gunakan User Label CubeMX (MUX_IN1_Pin, BTN_UP_Pin, dst.).
 * ============================================================ */

/* ============================================================
 * PEMETAAN PIN (acuan: dokumentasi_master_robot_line_follower_ted_2026.md)
 * ------------------------------------------------------------
 * Sensor Dariyan-X 16CH:
 *   Out1 = PA0 (ADC1_IN0)   Out2 = PA1 (ADC1_IN1)
 *   Out3 = PA2 (ADC1_IN2)   Out4 = PB1 (ADC1_IN9)
 *   In1  = PA3 (mux bit0)   In2  = PB0 (mux bit1)
 * OLED SH1106 (I2C1): SCL = PB6, SDA = PB7
 * Driver ZK-5AD (TIM1): D0=PA8, D1=PA9, D2=PA10, D3=PA11
 * Tombol (aktif LOW): UP=PA7, DOWN=PA6, OK=PA5, BACK=PA4
 * Debug USART1: TX=PA15, RX=PB3
 * ============================================================ */

/* ---- Sensor: kanal ADC (rank 1..4 = Out1..Out4) ---- */
#define SENS_ADC_CH_OUT1    ADC_CHANNEL_0   /* PA0 */
#define SENS_ADC_CH_OUT2    ADC_CHANNEL_1   /* PA1 */
#define SENS_ADC_CH_OUT3    ADC_CHANNEL_2   /* PA2 */
#define SENS_ADC_CH_OUT4    ADC_CHANNEL_9   /* PB1 */

/* ---- Sensor: pin selektor multiplexer ---- */
#define MUX_IN1_PORT        GPIOA
#define MUX_IN1_PIN         GPIO_PIN_3      /* PA3 = bit0 */
#define MUX_IN2_PORT        GPIOB
#define MUX_IN2_PIN         GPIO_PIN_0      /* PB0 = bit1 */

/* ---- OLED I2C1 ---- */
#define OLED_I2C            I2C1
#define OLED_SCL_PORT       GPIOB
#define OLED_SCL_PIN        GPIO_PIN_6
#define OLED_SDA_PORT       GPIOB
#define OLED_SDA_PIN        GPIO_PIN_7
#define OLED_I2C_AF         GPIO_AF4_I2C1

/* ---- Driver motor TIM1 ---- */
#define MOTOR_TIM           TIM1
#define MOTOR_D0_PORT       GPIOA
#define MOTOR_D0_PIN        GPIO_PIN_8      /* TIM1_CH1 */
#define MOTOR_D1_PORT       GPIOA
#define MOTOR_D1_PIN        GPIO_PIN_9      /* TIM1_CH2 */
#define MOTOR_D2_PORT       GPIOA
#define MOTOR_D2_PIN        GPIO_PIN_10     /* TIM1_CH3 */
#define MOTOR_D3_PORT       GPIOA
#define MOTOR_D3_PIN        GPIO_PIN_11     /* TIM1_CH4 */

/* ---- Tombol (input pull-up, aktif LOW) ---- */
#define BTN_PORT            GPIOA
#define BTN_UP_PIN          GPIO_PIN_7
#define BTN_DOWN_PIN        GPIO_PIN_6
#define BTN_OK_PIN          GPIO_PIN_5
#define BTN_BACK_PIN        GPIO_PIN_4

/* ---- Debug USART1 ---- */
#define DBG_UART            USART1
#define DBG_TX_PORT         GPIOA
#define DBG_TX_PIN          GPIO_PIN_15
#define DBG_RX_PORT         GPIOB
#define DBG_RX_PIN          GPIO_PIN_3
#define DBG_UART_AF         GPIO_AF7_USART1

/* ---- Sensor Dariyan-X 16CH ---- */
#define SENSOR_COUNT        16
#define SENSOR_GROUPS       4       /* Out1..Out4 -> PA0/PA1/PA2/PB1     */
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
