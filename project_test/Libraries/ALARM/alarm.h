#ifndef ALARM_H
#define ALARM_H

#include "stm32f10x.h"

typedef enum {
    STATE_WAIT_BLUETOOTH, // [NEW] 초기 블루투스 연결 대기
    STATE_IDLE,           // [NEW] 연결 완료 후 시간 설정 대기
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_WAIT_FOR_RAIN,
    STATE_ALARM_STOPPED
} AlarmState;

void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

// [수정] MM:SS 형식 변환 함수
void Time_Format(uint32_t total_seconds, char* buffer);

#endif
