#include "alarm.h"
#include "lcd.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_gpio.h"
#include <stdio.h>

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// 군대 기상나팔 주파수
#define NOTE_G  12000
#define NOTE_C  9000 
#define NOTE_E  7200 
#define NOTE_G2 6000 

uint16_t reveille_notes[] = { 
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G, 
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G,
    NOTE_G, NOTE_C, NOTE_G, NOTE_C, NOTE_G, NOTE_C,
    NOTE_E, NOTE_C, NOTE_G 
};

uint32_t reveille_beats[] = { 
    200, 200, 200, 200, 400, 
    200, 200, 200, 200, 400,
    100, 100, 100, 100, 100, 100,
    200, 200, 600 
};

// 부저 소리 (상태 변화 시 즉시 탈출)
static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    for (uint32_t i = 0; i < duration * 5; i++) {
        // 알람 상태가 STOPPED로 바뀌면 소리 즉시 끔
        if (*p_alarm_state != STATE_ALARM_ACTIVE) return; 
        
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

    if (*p_alarm_state == STATE_ALARM_ACTIVE) {
        Buzzer_Sound(reveille_notes[note_idx], reveille_beats[note_idx]);
        note_idx = (note_idx + 1) % num_notes;

        // 음 사이 대기 (여기서도 상태 확인)
        for (volatile int pause = 0; pause < 50000; pause++) {
            if (*p_alarm_state != STATE_ALARM_ACTIVE) {
                note_idx = 0;
                return;
            }
        }
    } else {
        note_idx = 0;
    }
}

void Alarm_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1; // 1초
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        LCD_Clear(WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char buf[30];
    static int32_t last_sec = -1;

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                sprintf(buf, "Wait: %02d sec", (int)*p_countdown_seconds);
                LCD_ShowString(40, 130, (u8*)buf, BLUE, WHITE);
            }
            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            break;

        case STATE_ALARM_ACTIVE:
            if (*p_elapsed_seconds == 0) {
                 LCD_ShowString(40, 100, (u8*)"WAKE UP!", RED, WHITE);
            }
            Play_Reveille();
            break;

        case STATE_ALARM_STOPPED:
            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            LCD_ShowString(40, 100, (u8*)"MISSION CLEAR!", BLUE, WHITE);
            break;

        case STATE_IDLE:
            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            break;
    }
}

AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_countdown_seconds = 0;
    *p_elapsed_seconds = 0;
    TIM_Cmd(TIM2, DISABLE);
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Ready...", BLACK, WHITE);
}