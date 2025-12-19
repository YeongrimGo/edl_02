#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"
#include "misc.h"
#include <stdio.h>

// 이 파일 내부에서만 사용할 변수들
static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

// 공개된 전역 변수 - 인터럽트 핸들러가 main.c에 있기 때문에 volatile로 선언
volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;


// 내부에서만 사용할 함수 프로토타입
static void RCC_Configure_Alarm(void);
static void GPIO_Configure_Alarm(void);
static void TIM_Configure_Alarm(void);

// --- 설정 함수 구현 ---

// RCC, GPIO, TIM 설정은 main.c의 다른 설정 함수와 병합될 것이므로
// 여기서는 Alarm_Init이 TIM만 설정하도록 단순화함.
// main.c에서 RCC와 GPIO 클럭을 이미 설정한다고 가정.
static void TIM_Configure_Alarm(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    // 1초마다 인터럽트 발생 설정
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}


// --- 공개 함수 구현 ---

void Alarm_Init(void) {
    // RCC와 GPIO 설정은 main.c에서 처리한다고 가정
    TIM_Configure_Alarm();
}

void Alarm_Start(uint16_t minutes) {
    if (minutes > 0) {
        *p_countdown_seconds = minutes * 60;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        
        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"알람 설정 완료", BLUE, WHITE);
        
        // 타이머 시작
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_displayed_second = -1;

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            {
                uint32_t remaining_total_seconds = *p_countdown_seconds;
                uint16_t rem_minutes = remaining_total_seconds / 60;
                uint16_t rem_seconds = remaining_total_seconds % 60;

                // 매초 화면을 업데이트하여 카운트다운 표시
                if (last_displayed_second != rem_seconds) {
                    last_displayed_second = rem_seconds;
                    sprintf(lcd_buffer, "남은 시간: %02d분 %02d초", rem_minutes, rem_seconds);
                    LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
                }
            }
            break;

        case STATE_ALARM_ACTIVE:
            // 알람이 울리기 시작하면 한 번만 화면 변경
            if (*p_elapsed_seconds == 1) {
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP~!", WHITE, RED);
            }
            break;

        case STATE_ALARM_STOPPED:
            {
                uint16_t minutes = *p_elapsed_seconds / 60;
                uint16_t seconds = *p_elapsed_seconds % 60;
                LCD_Clear(WHITE);
                sprintf(lcd_buffer, "경과: %d분 %d초", minutes, seconds);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
                // 상태 리셋은 main 루프에서 처리
            }
            break;

        case STATE_IDLE:
            // 대기 상태에서는 특별한 동작 없음
            break;
    }
}


// --- 외부 호출용 Getter/Setter 함수 ---

AlarmState Alarm_GetState(void) {
    return *p_alarm_state;
}

uint32_t Alarm_GetElapsedSeconds(void) {
    return *p_elapsed_seconds;
}

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    *p_countdown_seconds = 0;
    
    // 타이머가 돌고 있었다면 정지
    TIM_Cmd(TIM2, DISABLE);

    // 다음 알람을 위해 화면 초기화 (선택적)
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"알람 대기중...", BLUE, WHITE);
}