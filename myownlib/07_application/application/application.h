#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdint.h>

typedef enum {
    APPLICATION_IDLE = 0,
    APPLICATION_RUNNING,
    APPLICATION_LINE_LOST,
    APPLICATION_ADC_ERROR,
    APPLICATION_FAULT
} Application_State;

void Application_Init(void);
void Application_Process(void);
Application_State Application_GetState(void);

#endif
