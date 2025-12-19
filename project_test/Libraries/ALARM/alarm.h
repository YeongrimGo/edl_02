#ifndef __ALARM_H__
#define __ALARM_H__

#include "stm32f10x.h"
#include <stdint.h>

typedef enum {
    STATE_IDLE = 0,
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_ALARM_STOPPED
} AlarmState;

void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);

AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

#endif
