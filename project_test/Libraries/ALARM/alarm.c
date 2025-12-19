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

// 내부 함수 프로토타입
static void TIM_Configure_Alarm(void);

// --- 설정 함수 ---

static void TIM_Configure_Alarm(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    // 1초 인터럽트 설정 (SystemClock이 72MHz라고 가정)
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
    // 초기화 시 부저 끄기
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
}

// [수정됨] 입력받은 값을 '초(Seconds)' 단위로 바로 사용합니다.
// 예: 60을 입력하면 60초, 10을 입력하면 10초
void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        // [중요 수정] 곱하기 60을 제거했습니다. 받은 값 그대로 카운트다운 변수에 넣습니다.
        *p_countdown_seconds = seconds; 
        
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        
        // 시작 시 부저 끄기
        GPIO_ResetBits(GPIOB, GPIO_Pin_11);
        
        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        
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

                // 1초마다 화면 갱신
                if (last_displayed_second != rem_seconds) {
                    last_displayed_second = rem_seconds;
                    sprintf(lcd_buffer, "Time: %02d min %02d sec", rem_minutes, rem_seconds);
                    LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
                }
            }
            break;

        case STATE_ALARM_ACTIVE:
            // 알람 울림 상태: 부저 켜기 (PB11 High)
            GPIO_SetBits(GPIOB, GPIO_Pin_11);

            // 화면 갱신 (한 번만)
            if (*p_elapsed_seconds == 1) { 
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            break;

        case STATE_ALARM_STOPPED:
            {
                // 알람 정지 상태: 부저 끄기 (PB11 Low)
                GPIO_ResetBits(GPIOB, GPIO_Pin_11);

                uint16_t minutes = *p_elapsed_seconds / 60;
                uint16_t seconds = *p_elapsed_seconds % 60;
                LCD_Clear(WHITE);
                sprintf(lcd_buffer, "Elapsed: %d min %d sec", minutes, seconds);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
                // 상태 리셋은 main 루프에서 처리
            }
            break;

        case STATE_IDLE:
            // 대기 상태: 부저 끄기
            GPIO_ResetBits(GPIOB, GPIO_Pin_11);
            break;
    }
}

// --- Getter/Setter Functions ---

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
    
    // 리셋 시 부저 끄기
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    
    // 타이머 정지
    TIM_Cmd(TIM2, DISABLE);

    // 화면 초기화
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}