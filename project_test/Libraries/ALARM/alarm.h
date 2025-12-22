#ifndef ALARM_H
#define ALARM_H

#include "stm32f10x.h"

typedef enum {
    STATE_IDLE,             // 블루투스 연결 대기 (센서값 표시)
    STATE_BT_CONNECTED,     // [NEW] 연결됨, 숫자 입력 대기 (센서값 숨김, 안내문구)
    STATE_COUNTDOWN,        // 카운트다운 중 (reminder 표시)
    STATE_ALARM_ACTIVE,     // 알람 울림 (도망 모드)
    STATE_WAIT_FOR_RAIN,    // 빗물 감지 대기
    STATE_ALARM_STOPPED     // 종료
} AlarmState;

void Alarm_Init(void);
void Alarm_Start(uint16_t seconds);
void Alarm_Process(void);
void Alarm_Set_BT_Connected(void); // 연결 상태로 전환하는 함수
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

#endif
