#ifndef APP_H
#define APP_H

/* Pemilik: C (bersama). Dipanggil dari main.c di blok USER CODE. */
typedef enum {
    STATE_IDLE = 0,
    STATE_CALIB,
    STATE_READY,
    STATE_RUN,
    STATE_STOP
} AppState_t;

void       App_Init(void);
void       App_Update(void);        /* panggil di while(1) */
AppState_t App_GetState(void);

#endif /* APP_H */
