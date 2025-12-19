#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"
#include "misc.h"
#include <stdio.h>

// 내부 변수
static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

// 외부 공유 변수
volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// 딜레이 함수 (소리의 높낮이를 결정)
void Beep_Delay(volatile uint32_t count) {
    while (count--);
}

// --- 설정 함수 ---
static void TIM_Configure_Alarm(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

// --- 공용 함수 ---

void Alarm_Init(void) {
    TIM_Configure_Alarm();
    GPIO_ResetBits(GPIOB, GPIO_Pin_11); // 초기엔 끔
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds; // 초 단위 그대로 사용
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        
        GPIO_ResetBits(GPIOB, GPIO_Pin_11); // 시작할 땐 소리 끔
        
        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        
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

                if (last_displayed_second != rem_seconds) {
                    last_displayed_second = rem_seconds;
                    sprintf(lcd_buffer, "Time: %02d min %02d sec", rem_minutes, rem_seconds);
                    LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
                }
            }
            break;

        case STATE_ALARM_ACTIVE:
            // [수동 부저 핵심 코드]
            // 메인 루프가 이 부분을 매우 빠르게 반복 실행하며 "삐-" 소리를 만듭니다.
            GPIO_SetBits(GPIOB, GPIO_Pin_11);   // 핀 켜기
            Beep_Delay(2000);                   // 잠깐 대기
            GPIO_ResetBits(GPIOB, GPIO_Pin_11); // 핀 끄기
            Beep_Delay(2000);                   // 잠깐 대기
            
            // *참고: 소리가 너무 낮으면 숫자를 줄이고(예: 1000), 너무 높으면 늘리세요(예: 5000)

            // 화면 갱신 (한 번만)
            if (*p_elapsed_seconds == 1) { 
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            break;

        case STATE_ALARM_STOPPED:
            {
                GPIO_ResetBits(GPIOB, GPIO_Pin_11); // 확실히 끄기

                uint16_t minutes = *p_elapsed_seconds / 60;
                uint16_t seconds = *p_elapsed_seconds % 60;
                LCD_Clear(WHITE);
                sprintf(lcd_buffer, "Elapsed: %d min %d sec", minutes, seconds);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;

        case STATE_IDLE:
            GPIO_ResetBits(GPIOB, GPIO_Pin_11);
            break;
    }
}

// --- Getter/Setter Functions ---
AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }
void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    *p_countdown_seconds = 0;
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    TIM_Cmd(TIM2, DISABLE);
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}