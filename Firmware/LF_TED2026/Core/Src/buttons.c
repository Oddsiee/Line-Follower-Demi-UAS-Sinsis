#include "buttons.h"
#include "config.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * buttons.c — 4 tombol menu, aktif LOW (input pull-up)
 *
 * Pin (lihat config.h):
 *   UP   = PA7
 *   DOWN = PA6
 *   OK   = PA5
 *   BACK = PA4
 *
 * Debounce: sampel harus stabil DEBOUNCE_MS sebelum dianggap valid.
 * Long-press: ditahan >= LONGPRESS_MS.
 * ============================================================ */

#define DEBOUNCE_MS    20U
#define LONGPRESS_MS   800U

typedef struct {
	uint16_t pin;
	uint8_t  stable;      /* 1 = ditekan (LOW) */
	uint8_t  last_raw;
	uint32_t change_ms;
	uint32_t press_ms;
	uint8_t  long_fired;
} BtnState_t;

static BtnState_t s_btn[4];
static Button_t   s_pressed    = BTN_NONE;
static Button_t   s_longpressed = BTN_NONE;

static const uint16_t s_pins[4] = {
	BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN
};

void Buttons_Init(void)
{
	for (int i = 0; i < 4; i++) {
		s_btn[i].pin        = s_pins[i];
		s_btn[i].stable     = 0;
		s_btn[i].last_raw   = 0;
		s_btn[i].change_ms  = HAL_GetTick();
		s_btn[i].press_ms   = 0;
		s_btn[i].long_fired = 0;
	}
	s_pressed     = BTN_NONE;
	s_longpressed = BTN_NONE;
}

void Buttons_Update(void)
{
	uint32_t now = HAL_GetTick();

	for (int i = 0; i < 4; i++) {
		/* aktif LOW -> ditekan = RESET */
		uint8_t raw = (HAL_GPIO_ReadPin(BTN_PORT, s_btn[i].pin) == GPIO_PIN_RESET) ? 1U : 0U;

		if (raw != s_btn[i].last_raw) {
			s_btn[i].last_raw  = raw;
			s_btn[i].change_ms = now;
		}

		if ((now - s_btn[i].change_ms) >= DEBOUNCE_MS &&
			raw != s_btn[i].stable) {
			s_btn[i].stable = raw;

			if (raw) {
				/* baru ditekan */
				s_btn[i].press_ms   = now;
				s_btn[i].long_fired = 0;
				s_pressed = (Button_t)(BTN_UP + i);
			} else {
				/* dilepas */
				s_btn[i].press_ms = 0;
			}
		}

		/* long-press */
		if (s_btn[i].stable && !s_btn[i].long_fired && s_btn[i].press_ms) {
			if ((now - s_btn[i].press_ms) >= LONGPRESS_MS) {
				s_btn[i].long_fired = 1;
				s_longpressed = (Button_t)(BTN_UP + i);
			}
		}
	}
}

Button_t Buttons_Pressed(void)
{
	Button_t b = s_pressed;
	s_pressed = BTN_NONE;
	return b;
}

Button_t Buttons_LongPressed(void)
{
	Button_t b = s_longpressed;
	s_longpressed = BTN_NONE;
	return b;
}
