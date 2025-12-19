#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"

// 알람 상태 정의
typedef enum {
    STATE_IDLE,
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_ALARM_STOPPED
} AlarmState;

// [핵심] main.c가 변수를 알아볼 수 있게 공유(extern)
extern volatile AlarmState* p_alarm_state;
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;

// 함수 선언 (목차)
void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
void Alarm_Reset(void);

// 값을 가져오는 함수들
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);

// 멜로디 함수
void Play_Reveille(void);

#endif