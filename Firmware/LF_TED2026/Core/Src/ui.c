#include "ui.h"

/* TODO (M4/M7, pemilik C): U8g2 SH1106 via HAL I2C1 (alamat 0x3C).
 * Layar: status, menu, edit PID, edit speed, kalibrasi, data live. */

void UI_Init(void) { }

void UI_Update(AppState_t state, Params_t *p,
               int16_t position, int16_t left, int16_t right)
{
    (void)state; (void)p; (void)position; (void)left; (void)right;
}
