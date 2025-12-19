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

// [핵심] 다른 파일(main.c, it.c)에서 변수들을 쓸 수 있게 공유(extern)
extern volatile AlarmState* p_alarm_state;
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;

// 함수 선언
void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
void Alarm_Reset(void);
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Play_Reveille(void); // alarm.c에 있는 경우 선언 추가

#endif