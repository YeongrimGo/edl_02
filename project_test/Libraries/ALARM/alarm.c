#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_tim.h"
#include <stdio.h>

#define RAIN_THRESHOLD 1500 // 빗물 감지 임계값 (환경에 따라 조절)

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;
volatile uint8_t is_rain_mode_active = 0; // 터치 후 1로 변경됨

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

extern volatile uint32_t ADC_Value[2]; // 0:기존, 1:빗물(PA1)

// 군대 기상나팔 음계 및 박자
uint16_t reveille_notes[] = {2500, 1800, 1500, 1800, 2500, 2500, 1800, 1500, 1800, 2500};
uint32_t reveille_beats[] = {150, 150, 150, 150, 300, 150, 150, 150, 150, 300};

static void Play_Reveille_Step(void) {
    static int note_idx = 0;
    static int sub_step = 0;

    // 부저 토글로 소리 생성
    for (volatile int i = 0; i < reveille_notes[note_idx]; i++);
    GPIO_WriteBit(GPIOB, GPIO_Pin_0, (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_0)));

    sub_step++;
    if (sub_step > reveille_beats[note_idx]) {
        sub_step = 0;
        note_idx = (note_idx + 1) % 10;
        for (volatile int p = 0; p < 10000; p++); // 음 간격
    }
}

void Alarm_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1; // 10kHz
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;  // 1초 주기
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

void Alarm_Start(uint16_t seconds) {
    *p_countdown_seconds = seconds;
    *p_alarm_state = STATE_COUNTDOWN;
    *p_elapsed_seconds = 0;
    is_rain_mode_active = 0;
    TIM_Cmd(TIM2, ENABLE);
}

void Alarm_Process(void) {
    char buf[32];

    // [로직] 알람 중 터치(PC1) 감지 시 -> 빗물 모드 활성화
    if (*p_alarm_state == STATE_ALARM_ACTIVE && is_rain_mode_active == 0) {
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
            is_rain_mode_active = 1;
        }
    }

    // [로직] 빗물 모드 활성화 중 물(PA1) 감지 시 -> 알람 종료
    if (is_rain_mode_active && ADC_Value[1] < RAIN_THRESHOLD) {
        *p_alarm_state = STATE_ALARM_STOPPED;
        TIM_Cmd(TIM2, DISABLE);
    }

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            sprintf(buf, "Countdown: %d sec", (int)*p_countdown_seconds);
            LCD_ShowString(40, 130, (u8*)buf, BLUE, WHITE);
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 OFF
            break;

        case STATE_ALARM_ACTIVE:
            Play_Reveille_Step(); // 기상나팔 연주
            LCD_ShowString(40, 100, (u8*)"!!! WAKE UP !!!", WHITE, RED);
            if (is_rain_mode_active)
                LCD_ShowString(40, 160, (u8*)"RAIN MODE: ON ", GREEN, RED);
            else
                LCD_ShowString(40, 160, (u8*)"TOUCH TO ARM  ", WHITE, RED);
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 OFF
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
    *p_countdown_seconds = 0;
    *p_elapsed_seconds = 0;
    is_rain_mode_active = 0;
    TIM_Cmd(TIM2, DISABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_0);
    LCD_Clear(WHITE);
}
