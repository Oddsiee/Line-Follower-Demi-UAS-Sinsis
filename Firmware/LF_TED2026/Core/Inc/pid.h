#ifndef PID_H
#define PID_H

/* Pemilik: A */
typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float integral_limit;
    float out_limit;
    int   first;
} PID_t;

void  PID_Init(PID_t *p, float kp, float ki, float kd, float out_limit);
void  PID_Reset(PID_t *p);
float PID_Compute(PID_t *p, float error, float dt_s);

#endif /* PID_H */
