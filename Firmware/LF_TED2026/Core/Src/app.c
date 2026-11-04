#include "app.h"
#include "config.h"
#include "params.h"
#include "sensor.h"
#include "calibration.h"
#include "motor.h"
#include "pid.h"
#include "track.h"
#include "ui.h"
#include "buttons.h"
#include "storage.h"
#include "debug.h"

/* State machine utama. Isi logika bertahap sesuai milestone. */

static AppState_t s_state = STATE_IDLE;
static Params_t   s_params;
static PID_t      s_pid;

void App_Init(void)
{
    Debug_Init();
    Sensor_Init();
    Motor_Init();
    Buttons_Init();
    UI_Init();
    Track_Init();

    if (!Storage_Load(&s_params)) Params_SetDefaults(&s_params);
    PID_Init(&s_pid, s_params.kp, s_params.ki, s_params.kd, (float)MOTOR_CMD_MAX);
    s_state = STATE_IDLE;
}

void App_Update(void)
{
    uint16_t raw[SENSOR_COUNT];
    uint16_t norm[SENSOR_COUNT];

    Buttons_Update();

    switch (s_state) {
    case STATE_IDLE:
        /* menu, edit parameter, simpan ke Flash (M7) */
        break;

    case STATE_CALIB:
        Sensor_ReadRaw(raw);
        Calib_Update(raw);
        break;

    case STATE_READY:
        /* menunggu tombol OK untuk mulai */
        break;

    case STATE_RUN:
        Sensor_ReadRaw(raw);
        Calib_Normalize(&s_params, raw, norm);
        (void)Track_Update(norm, &s_params);
        /* M8: error = Sensor_GetPosition(); out = PID_Compute(...);
         *     Motor_Set(base - out, base + out); */
        break;

    case STATE_STOP:
        Motor_Brake();
        break;
    }

    UI_Update(s_state, &s_params, Sensor_GetPosition(), 0, 0);
}

AppState_t App_GetState(void) { return s_state; }
