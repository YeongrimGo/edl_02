#include "alarm.h"
#include "lcd.h"
#include "inc/hw_config.h"
#include "stm32f10x_tim.h"
#include <stdio.h>

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds   = &elapsed_seconds;
volatile AlarmState* p_alarm_state     = &alarm_state;

void Alarm_Init(void) {
    // TIM2 설정 (1초 인터럽트)
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    TIM_TimeBaseStructure.TIM_Prescaler     = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period        = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // 부저는 기본 OFF
    BUZZER_Stop();
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        BUZZER_Stop();
        LCD_Clear(WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_displayed_second = -1;
    static uint8_t buzzer_started = 0;

    switch (*p_alarm_state) {
    case STATE_COUNTDOWN:
        buzzer_started = 0;
        if (last_displayed_second != (int32_t)*p_countdown_seconds) {
            last_displayed_second = (int32_t)*p_countdown_seconds;
            sprintf(lcd_buffer, "Time: %02d:%02d",
                    (int)(*p_countdown_seconds / 60),
                    (int)(*p_countdown_seconds % 60));
            LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
        }
        break;

    case STATE_ALARM_ACTIVE:
        if (!buzzer_started) {
            buzzer_started = 1;
            BUZZER_Start(2000); // 2kHz (필요하면 1000~4000 사이로 조절)
            LCD_Clear(RED);
            LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
        }
        break;

    case STATE_ALARM_STOPPED:
        buzzer_started = 0;
        BUZZER_Stop();
        break;

    default:
        buzzer_started = 0;
        BUZZER_Stop();
        break;
    }
}

AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    *p_countdown_seconds = 0;

    BUZZER_Stop();
    TIM_Cmd(TIM2, DISABLE);

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}
