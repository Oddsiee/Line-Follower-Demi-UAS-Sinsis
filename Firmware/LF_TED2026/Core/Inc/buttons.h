#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

/* Pemilik: C. Tombol aktif LOW (input pull-up). */
typedef enum { BTN_NONE = 0, BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK } Button_t;

void     Buttons_Init(void);
void     Buttons_Update(void);                 /* panggil rutin (mis. 5 ms) */
Button_t Buttons_Pressed(void);                /* event tekan singkat       */
Button_t Buttons_LongPressed(void);            /* event tahan lama          */

#endif /* BUTTONS_H */
