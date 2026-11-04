#include "pid.h"

void PID_Init(PID_t *p, float kp, float ki, float kd, float out_limit)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    p->out_limit = out_limit;
    p->integral_limit = out_limit;
    PID_Reset(p);
}

void PID_Reset(PID_t *p)
{
    p->integral = 0.0f;
    p->prev_error = 0.0f;
    p->first = 1;
}

float PID_Compute(PID_t *p, float error, float dt_s)
{
    float derivative = 0.0f;

    if (dt_s <= 0.0f) dt_s = 0.001f;

    p->integral += error * dt_s;
    if (p->integral >  p->integral_limit) p->integral =  p->integral_limit;
    if (p->integral < -p->integral_limit) p->integral = -p->integral_limit;

    if (!p->first) derivative = (error - p->prev_error) / dt_s;
    p->first = 0;
    p->prev_error = error;

    float out = p->kp * error + p->ki * p->integral + p->kd * derivative;
    if (out >  p->out_limit) out =  p->out_limit;
    if (out < -p->out_limit) out = -p->out_limit;
    return out;
}
