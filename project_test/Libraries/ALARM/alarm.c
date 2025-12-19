#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "inc/hw_config.h" // Sensor_Mode_Reset 사용
#include <stdio.h>

// [추가] main.c에 선언된 DMA ADC 버퍼를 가져옴
extern volatile uint32_t ADC_Value[1];

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// --- 기상나팔 멜로디 데이터 ---
#define NOTE_G  3000
#define NOTE_C  2250
#define NOTE_E  1800
#define NOTE_G2 1500

uint16_t reveille_notes[] = {
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G,
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G,
    NOTE_G, NOTE_C, NOTE_G, NOTE_C, NOTE_G, NOTE_C,
    NOTE_E, NOTE_C, NOTE_G
};
uint32_t reveille_beats[] = {
    100, 100, 100, 100, 200,
    100, 100, 100, 100, 200,
    50, 50, 50, 50, 50, 50,
    100, 100, 300
};

static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    for (uint32_t i = 0; i < duration; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);

        // 알람이 꺼졌을 때만 탈출
        if (*p_alarm_state == STATE_ALARM_STOPPED || *p_alarm_state == STATE_IDLE) return;
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

    // Active 상태이거나 빗물을 기다리는 상태(세수하러 가는 중)이면 소리 재생
    if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
        Buzzer_Sound(reveille_notes[note_idx], reveille_beats[note_idx]);
        note_idx = (note_idx + 1) % num_notes;
        for (volatile int pause = 0; pause < 50000; pause++);
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
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        // 시작 시 센서 상태 확실히 초기화 (터치 켬, 빗물 끔)
        Sensor_Mode_Reset();

        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_sec = -1;

    // 화면 깜빡임 방지 및 상태 변경 감지 변수
    static AlarmState last_state = STATE_IDLE;

    // [중요] 빗물 감지 모드 진입 후 안정화 대기 카운터
    static uint32_t stability_count = 0;

    // 상태가 변경되었을 때 화면 초기화 및 변수 리셋
    if (last_state != *p_alarm_state) {
        switch (*p_alarm_state) {
            case STATE_WAIT_FOR_RAIN:
                LCD_Clear(YELLOW);
                LCD_ShowString(40, 50, (u8*)"WAITING RAIN...", BLACK, YELLOW);
                LCD_ShowString(40, 100, (u8*)"GO WASH FACE!", BLACK, YELLOW);

                // [리셋] 상태 진입 시 카운터 초기화
                stability_count = 0;
                break;
            case STATE_ALARM_STOPPED:
                LCD_Clear(WHITE);
                break;
            default:
                break;
        }
        last_state = *p_alarm_state;
    }

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                sprintf(lcd_buffer, "Remaining: %02d sec", (int)last_sec);
                LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            break;

        case STATE_ALARM_ACTIVE:
            if (*p_elapsed_seconds == 0) {
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            Play_Reveille();
            break;

        case STATE_WAIT_FOR_RAIN:
            // [POLLING] 빗물 대기 상태
            {
                // DMA 버퍼에서 값 읽기 (main.c의 ADC_Value)
                uint16_t adc_val = (uint16_t)ADC_Value[0];

                // 화면에 현재 센서 값 출력 (디버깅용)
                sprintf(lcd_buffer, "Rain Sensor: %04d", adc_val);
                LCD_ShowString(40, 150, (u8*)lcd_buffer, BLACK, YELLOW);

                // [수정 핵심] 진입 후 일정 시간(약 1~2초) 동안은 감지 무시
                // 루프 속도에 따라 값 조정 필요 (현재 약 50만 루프)
                if (stability_count < 200000) {
                    stability_count++;
                }
                else {
                    // 안정화 이후 실제 감지 시작
                    // 현재: 값이 2000 미만으로 떨어지면 비가 온다고 판단 (Wet < Dry)
                    if (adc_val < 2000) {
                        *p_alarm_state = STATE_ALARM_STOPPED;
                        TIM_Cmd(TIM2, DISABLE);
                        USART2_SendString("\r\nRain Detected! Alarm Stopped.\r\n");
                    }
                }

                Play_Reveille(); // 알람은 계속 울림
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 소리 끄기
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

    // 센서 상태 초기화
    Sensor_Mode_Reset();

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}
