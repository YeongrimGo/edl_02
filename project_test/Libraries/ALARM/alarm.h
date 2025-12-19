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

// 전역 변수 공유 (main.c, it.c에서 사용)
extern volatile AlarmState* p_alarm_state;
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;

// 함수 선언 (Prototype)
void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
void Alarm_Reset(void);

// 상태 및 시간 확인 함수
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);

// 멜로디 재생 함수
void Play_Reveille(void);

#endif