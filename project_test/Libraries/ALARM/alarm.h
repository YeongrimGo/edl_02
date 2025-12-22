#ifndef ALARM_H
#define ALARM_H

#include "stm32f10x.h"

typedef enum {
    STATE_IDLE,
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_WAIT_FOR_RAIN, // [NEW] 터치 후 빗물 감지 대기 상태
    STATE_ALARM_STOPPED
} AlarmState;

void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

#endif
