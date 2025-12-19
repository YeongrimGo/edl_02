#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"
#include <stdint.h>

typedef enum {
    STATE_IDLE,
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_ALARM_STOPPED
} AlarmState;

// 전역 변수 포인터 (ISR 및 메인 루프 공유)
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;
extern volatile uint8_t is_rain_mode_active;

void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

#endif
