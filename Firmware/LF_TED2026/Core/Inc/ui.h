#ifndef UI_H
#define UI_H

#include "params.h"
#include "app.h"

/* Pemilik: C. OLED SH1106 via U8g2 (bukan SSD1306). */
typedef enum {
    UI_SCREEN_STATUS = 0,
    UI_SCREEN_MENU,
    UI_SCREEN_EDIT_PID,
    UI_SCREEN_EDIT_SPEED,
    UI_SCREEN_CALIB,
    UI_SCREEN_LIVE
} UiScreen_t;

void UI_Init(void);
void UI_Update(AppState_t state, Params_t *p,
               int16_t position, int16_t left, int16_t right);

#endif /* UI_H */
