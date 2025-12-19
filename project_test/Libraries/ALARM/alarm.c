#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include <stdio.h>

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

void Beep_Delay(volatile uint32_t count) { while (count--); }

void Alarm_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    GPIO_SetBits(GPIOC, GPIO_Pin_6); // OFF (High)
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        GPIO_SetBits(GPIOC, GPIO_Pin_6); 
        LCD_Clear(WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_displayed_second = -1;

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_displayed_second != (int32_t)*p_countdown_seconds) {
                last_displayed_second = *p_countdown_seconds;
                sprintf(lcd_buffer, "Time: %02d:%02d", (int)(*p_countdown_seconds/60), (int)(*p_countdown_seconds%60));
                LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;

        case STATE_ALARM_ACTIVE:
            // [수동 부저 떨림 발생] PC6 사용
            for(int i=0; i<150; i++) {
                GPIO_ResetBits(GPIOC, GPIO_Pin_6); // ON (Low)
                Beep_Delay(700); 
                GPIO_SetBits(GPIOC, GPIO_Pin_6);   // OFF (High)
                Beep_Delay(700); 
            }

            if (*p_elapsed_seconds == 1) { 
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOC, GPIO_Pin_6); 
            break;

        default:
            GPIO_SetBits(GPIOC, GPIO_Pin_6);
            break;
    }
}

AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }
void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    GPIO_SetBits(GPIOC, GPIO_Pin_6); 
    TIM_Cmd(TIM2, DISABLE);
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}