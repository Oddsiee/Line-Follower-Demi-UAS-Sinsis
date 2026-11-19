#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * buttons.h — Driver 4 tombol menu anti-blocking (sampling 1 kHz)
 * Pin (aktif LOW, internal pull-up):
 *   UP   = PA7
 *   DOWN = PA6
 *   OK   = PA5
 *   BACK = PA4
 * ============================================================ */

typedef enum {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_OK,
    BTN_BACK
} Button_t;

void     Buttons_Init(void);
void     Buttons_Tick1ms(void); /* Panggil 1 kHz dari SysTick_Handler */
bool     Buttons_PopEvent(Button_t *btn, uint8_t *is_long);
void     Buttons_Drain(void);
uint8_t  Buttons_GetRawMask(void); /* Bit 0=UP, 1=DOWN, 2=OK, 3=BACK */
Button_t Buttons_Pressed(void);
Button_t Buttons_LongPressed(void);

#endif /* BUTTONS_H */
