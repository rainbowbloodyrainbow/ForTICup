#ifndef PID_H
#define PID_H

#include <stdbool.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float integralMinimum;
    float integralMaximum;
    float outputMinimum;
    float outputMaximum;
    float derivativeFilterCoefficient;
} PID_Config;

typedef struct {
    PID_Config config;
    float integral;
    float previousError;
    float filteredDerivative;
    float lastOutput;
    bool hasPreviousError;
    bool initialized;
} PID;

bool PID_Init(PID *pid, const PID_Config *config);
void PID_Reset(PID *pid);
float PID_UpdateError(
    PID *pid, float error, float dtSeconds);
float PID_Update(
    PID *pid,
    float target,
    float measurement,
    float dtSeconds);
float PID_GetLastOutput(const PID *pid);

#endif
