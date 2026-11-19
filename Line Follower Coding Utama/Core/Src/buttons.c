#include "buttons.h"
#include "config.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * buttons.c — Sampling tombol 1 kHz (SysTick) + FIFO 8-slot
 * Mencegah tombol terlewat saat transfer I2C OLED (~110 ms).
 * ============================================================ */

#define BTN_COUNT         4U
#define BTN_DEBOUNCE_MS   12U
#define BTN_LONGPRESS_MS  500U
#define BTN_QUEUE_MASK    7U

typedef struct {
    uint8_t btn;
    uint8_t is_long;
} BtnEvt_t;

typedef struct {
    uint8_t  stable;
    uint8_t  last_raw;
    uint8_t  stable_cnt;
    uint8_t  long_fired;
    uint32_t press_ms;
} BtnFilter_t;

static const uint16_t s_pins[BTN_COUNT] = {
    BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN
};

static volatile BtnEvt_t s_evt_q[8];
static volatile uint8_t  s_evt_head    = 0U;
static volatile uint8_t  s_evt_tail    = 0U;
static volatile uint8_t  s_raw_mask    = 0U;
static volatile uint8_t  s_btn_ready   = 0U;
static volatile Button_t s_pressed     = BTN_NONE;
static volatile Button_t s_longpressed = BTN_NONE;
static BtnFilter_t       s_filter[BTN_COUNT];

static void push_btn_evt(uint8_t btn, uint8_t is_long)
{
    uint8_t next = (uint8_t)((s_evt_head + 1U) & BTN_QUEUE_MASK);
    if (next != s_evt_tail) {
        s_evt_q[s_evt_head].btn     = btn;
        s_evt_q[s_evt_head].is_long = is_long;
        s_evt_head = next;
    }

    if (is_long) {
        s_longpressed = (Button_t)btn;
    } else {
        s_pressed = (Button_t)btn;
    }
}

void Buttons_Init(void)
{
    GPIO_InitTypeDef g = {0};

    s_btn_ready = 0U;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Konfigurasi eksplisit keempat tombol (PA4..PA7) sebagai input pull-up */
    g.Pin   = BTN_UP_PIN | BTN_DOWN_PIN | BTN_OK_PIN | BTN_BACK_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BTN_PORT, &g);

    for (uint8_t i = 0U; i < BTN_COUNT; i++) {
        s_filter[i].stable     = 0U;
        s_filter[i].last_raw   = 0U;
        s_filter[i].stable_cnt = 0U;
        s_filter[i].long_fired = 0U;
        s_filter[i].press_ms   = 0U;
    }

    s_evt_head    = 0U;
    s_evt_tail    = 0U;
    s_raw_mask    = 0U;
    s_pressed     = BTN_NONE;
    s_longpressed = BTN_NONE;
    s_btn_ready   = 1U;
}

void Buttons_Tick1ms(void)
{
    if (!s_btn_ready) {
        return;
    }

    uint32_t now  = HAL_GetTick();
    uint8_t  mask = 0U;

    for (uint8_t i = 0U; i < BTN_COUNT; i++) {
        uint8_t raw = (HAL_GPIO_ReadPin(BTN_PORT, s_pins[i]) == GPIO_PIN_RESET) ? 1U : 0U;
        if (raw) {
            mask |= (uint8_t)(1U << i);
        }

        if (raw != s_filter[i].last_raw) {
            s_filter[i].last_raw   = raw;
            s_filter[i].stable_cnt = 0U;
        } else if (raw != s_filter[i].stable) {
            if (++s_filter[i].stable_cnt >= BTN_DEBOUNCE_MS) {
                s_filter[i].stable     = raw;
                s_filter[i].stable_cnt = 0U;
                if (raw) {
                    s_filter[i].press_ms   = (now != 0U) ? now : 1U;
                    s_filter[i].long_fired = 0U;
                } else {
                    if ((s_filter[i].press_ms != 0U) && !s_filter[i].long_fired) {
                        push_btn_evt((uint8_t)(BTN_UP + i), 0U);
                    }
                    s_filter[i].press_ms = 0U;
                }
            }
        }

        if (s_filter[i].stable && !s_filter[i].long_fired && (s_filter[i].press_ms != 0U)) {
            if ((now - s_filter[i].press_ms) >= BTN_LONGPRESS_MS) {
                s_filter[i].long_fired = 1U;
                push_btn_evt((uint8_t)(BTN_UP + i), 1U);
            }
        }
    }

    s_raw_mask = mask;
}

bool Buttons_PopEvent(Button_t *btn, uint8_t *is_long)
{
    if (s_evt_tail == s_evt_head) {
        return false;
    }
    if (btn != NULL) {
        *btn = (Button_t)s_evt_q[s_evt_tail].btn;
    }
    if (is_long != NULL) {
        *is_long = s_evt_q[s_evt_tail].is_long;
    }
    s_evt_tail = (uint8_t)((s_evt_tail + 1U) & BTN_QUEUE_MASK);
    return true;
}

void Buttons_Drain(void)
{
    s_evt_tail    = s_evt_head;
    s_pressed     = BTN_NONE;
    s_longpressed = BTN_NONE;
}

uint8_t Buttons_GetRawMask(void)
{
    return s_raw_mask;
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
