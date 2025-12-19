#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include <stdio.h>

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// --- 기상나팔 멜로디 데이터 ---
// 음계 주파수(루프 지연 값으로 근사치 조절)
#define NOTE_G  12000 // 기존 3000 -> 12000
#define NOTE_C  9000  // 기존 2250 -> 9000
#define NOTE_E  7200  // 기존 1800 -> 7200
#define NOTE_G2 6000  // 기존 1500 -> 6000

uint16_t reveille_notes[] = {
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G,
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G,
    NOTE_G, NOTE_C, NOTE_G, NOTE_C, NOTE_G, NOTE_C,
    NOTE_E, NOTE_C, NOTE_G
};

// 각 음의 길이 (단위: 루프 횟수)
uint32_t reveille_beats[] = {
    200, 200, 200, 200, 400,
    200, 200, 200, 200, 400,
    100, 100, 100, 100, 100, 100,
    200, 200, 600
};

static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    // duration 루프 횟수만큼 파형 생성
    // pitch 값이 클수록 딜레이가 길어져서 저음이 남
    for (uint32_t i = 0; i < duration * 10; i++) { // 지속 시간도 조금 늘림 (*10)

        GPIO_SetBits(GPIOB, GPIO_Pin_0); // High
        for (volatile int d = 0; d < pitch; d++); // Delay

        GPIO_ResetBits(GPIOB, GPIO_Pin_0); // Low
        for (volatile int d = 0; d < pitch; d++); // Delay

        // 알람 정지 시 즉시 탈출 (반응성 향상)
        if (*p_alarm_state != STATE_ALARM_ACTIVE) return;
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

    if (*p_alarm_state == STATE_ALARM_ACTIVE) {
        Buzzer_Sound(reveille_notes[note_idx], reveille_beats[note_idx]);

        note_idx = (note_idx + 1) % num_notes;

        // 음과 음 사이 짧은 간격 (Staccato 느낌)
        for (volatile int pause = 0; pause < 100000; pause++);
    } else {
        note_idx = 0;
    }
}

void Alarm_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds; // 분 단위 연산 제거 -> 초 단위로 직접 입력
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_sec = -1;

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                sprintf(lcd_buffer, "Remaining: %02d sec", (int)last_sec);
                LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 끔 (High Active인 경우 ResetBits로 변경 필요)
            break;

        case STATE_ALARM_ACTIVE:
            if (*p_elapsed_seconds == 0) { // 진입 직후 한 번만
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            Play_Reveille(); // 군대 기상나팔 연주
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            sprintf(lcd_buffer, "Stopped: %d sec", (int)*p_elapsed_seconds);
            LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            break;
    }
}

AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    *p_countdown_seconds = 0;
    TIM_Cmd(TIM2, DISABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_0);
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}
