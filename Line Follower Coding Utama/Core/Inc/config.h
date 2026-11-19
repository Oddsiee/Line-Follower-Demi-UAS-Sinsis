#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ============================================================
 * config.h — konstanta hardware & pinout STM32F401CCU6
 * Mengacu pada dokumentasi_master_robot_line_follower_ted_2026.md
 * ============================================================ */

/* ---- Sensor: kanal ADC (Out1..Out4) ---- */
#define SENS_ADC_CH_OUT1    ADC_CHANNEL_0   /* PA0 */
#define SENS_ADC_CH_OUT2    ADC_CHANNEL_1   /* PA1 */
#define SENS_ADC_CH_OUT3    ADC_CHANNEL_2   /* PA2 */
#define SENS_ADC_CH_OUT4    ADC_CHANNEL_9   /* PB1 */

/* ---- Sensor: pin selektor multiplexer ---- */
#define MUX_IN1_PORT        GPIOA
#define MUX_IN1_PIN         GPIO_PIN_3      /* PA3 = bit0 */
#define MUX_IN2_PORT        GPIOB
#define MUX_IN2_PIN         GPIO_PIN_0      /* PB0 = bit1 */

/* ---- OLED SH1106 (I2C1) ---- */
#define OLED_I2C            I2C1
#define OLED_SCL_PORT       GPIOB
#define OLED_SCL_PIN        GPIO_PIN_6
#define OLED_SDA_PORT       GPIOB
#define OLED_SDA_PIN        GPIO_PIN_7
#define OLED_I2C_AF         GPIO_AF4_I2C1

/* ---- Driver motor ZK-5AD (TIM1 CH1..CH4) ---- */
#define MOTOR_TIM           TIM1
#define MOTOR_D0_PORT       GPIOA
#define MOTOR_D0_PIN        GPIO_PIN_8      /* TIM1_CH1 (Kanan IN2 / Mundur) */
#define MOTOR_D1_PORT       GPIOA
#define MOTOR_D1_PIN        GPIO_PIN_9      /* TIM1_CH2 (Kanan IN1 / Maju)   */
#define MOTOR_D2_PORT       GPIOA
#define MOTOR_D2_PIN        GPIO_PIN_10     /* TIM1_CH3 (Kiri IN2 / Mundur)  */
#define MOTOR_D3_PORT       GPIOA
#define MOTOR_D3_PIN        GPIO_PIN_11     /* TIM1_CH4 (Kiri IN1 / Maju)    */

/* ---- Tombol navigasi (input pull-up, aktif LOW) ---- */
#define BTN_PORT            GPIOA
#define BTN_UP_PIN          GPIO_PIN_7
#define BTN_DOWN_PIN        GPIO_PIN_6
#define BTN_OK_PIN          GPIO_PIN_5
#define BTN_BACK_PIN        GPIO_PIN_4

/* ---- LED onboard (aktif LOW) ---- */
#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13

/* ---- Sensor Dariyan-X 16CH ---- */
#define SENSOR_COUNT          16
#define SENSOR_GROUPS         4
#define MUX_STATES            4
#define MUX_SETTLE_US         50U           /* settling mux terverifikasi M2 */
#define SENSOR_OVERSAMPLE     4U            /* rata-rata 4x per state mux    */
#define ADC_MAX               4095
#define SENSOR_NORM_MAX       1000
#define SENSOR_POS_MAX        7500
#define SENSOR_POS_SCALE      1000
#define SENSOR_ON_LINE_MIN    300
#define SENSOR_CALIB_MIN_SPAN 300U          /* ambang kontras kalibrasi baik */

/* ---- Default parameter PID & kecepatan ---- */
#define DEFAULT_KP            0.05f
#define DEFAULT_KI            0.0f
#define DEFAULT_KD            0.50f
#define DEFAULT_BASE_SPEED    400
#define DEFAULT_MAX_SPEED     800

/* ---- Motor (ZK-5AD, TIM1) ---- */
#define MOTOR_CMD_MAX       1000            /* rentang -1000..+1000 */
#define MOTOR_PWM_ARR       999             /* ARR TIM1 = 999       */

/* ---- Penyimpanan Flash (STM32F401CCU6: 256 KB, Sektor 5 = 128 KB) ---- */
#define FLASH_PARAMS_SECTOR FLASH_SECTOR_5
#define FLASH_PARAMS_ADDR   0x08020000UL
#define PARAMS_MAGIC        0x4C465432UL    /* "LFT2" */

#endif /* CONFIG_H */
