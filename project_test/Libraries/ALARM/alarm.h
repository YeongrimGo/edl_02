#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"
#include <stdint.h>

// 알람 상태를 관리하기 위한 열거형
typedef enum {
    STATE_IDLE,
    STATE_COUNTDOWN,
    STATE_ALARM_ACTIVE,
    STATE_ALARM_STOPPED
} AlarmState;

// 외부에서 호출할 함수 프로토타입 선언
void Alarm_Init(void);
void Alarm_Start(uint16_t minutes);
void Alarm_Process(void);
AlarmState Alarm_GetState(void);
uint32_t Alarm_GetElapsedSeconds(void);
void Alarm_Reset(void);

#endif /* __ALARM_H */