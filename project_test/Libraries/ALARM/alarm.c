#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "inc/hw_config.h"
#include <stdio.h>

extern volatile uint32_t ADC_Value[1];

static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// --- 기상나팔 멜로디 ---
#define NOTE_G  3000
#define NOTE_C  2250
#define NOTE_E  1800

uint16_t reveille_notes[] = {
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G
};
uint32_t reveille_beats[] = {
    100, 100, 100, 100, 200
};

static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    for (uint32_t i = 0; i < duration; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);

        if (*p_alarm_state == STATE_ALARM_STOPPED || *p_alarm_state == STATE_IDLE) return;
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

    if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
        Buzzer_Sound(reveille_notes[note_idx], reveille_beats[note_idx]);
        note_idx = (note_idx + 1) % num_notes;
    } else {
        note_idx = 0;
    }
}

void Alarm_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        Sensor_Mode_Reset();
        Motor_Stop(); // 알람 설정 시 모터 정지

        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

// alarm.c 파일 내부 수정

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;

    // [수정] 소리 재생 빈도 조절을 위한 카운터 변수
    static uint32_t sound_tick = 0;

    // 초음파 센서 변수
    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    static uint32_t sensor_timer = 0;

    // 장애물 인식 거리 (cm)
    const uint32_t OBS_THRESHOLD = 25;

    // 상태 변경 시 화면 초기화 로직 (기존과 동일)
    if (last_state != *p_alarm_state) {
        if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
            LCD_Clear(YELLOW);
            LCD_ShowString(40, 50, (u8*)"WAIT RAIN...", BLACK, YELLOW);
            stability_count = 0;
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_ALARM_STOPPED) {
            LCD_Clear(WHITE);
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
             LCD_Clear(RED);
        }
        else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop();
             LCD_Clear(WHITE);
             LCD_ShowString(40, 50, (u8*)"[IDLE MODE]", BLACK, WHITE);
             LCD_ShowString(40, 80, (u8*)"Sensor Check", BLUE, WHITE);
        }
        last_state = *p_alarm_state;
    }

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                sprintf(lcd_buffer, "Rem: %02d sec", (int)last_sec);
                LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            // === 도망가는 알람 모드 (자율주행 개선) ===
            LCD_ShowString(40, 50, (u8*)"RUNAWAY ALARM!", WHITE, RED);

            // 1. 센서 측정 (간섭 방지를 위해 측정 사이에 미세한 딜레이 추가)
            dist_L = Get_Ultrasonic_Dist(1);
            for(volatile int i=0; i<10000; i++); // 단순 지연

            dist_C = Get_Ultrasonic_Dist(2);
            for(volatile int i=0; i<10000; i++); // 단순 지연

            dist_R = Get_Ultrasonic_Dist(3);

            sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
            LCD_ShowString(20, 110, (u8*)lcd_buffer, YELLOW, RED);

            // 2. 주행 로직 (0이 나오면 센서 에러나 먼 거리이므로 전진하지 않도록 방어 코드 추가 가능)
            // 우선순위: 중앙 -> 왼쪽 -> 오른쪽
            if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                Motor_Backward();
                LCD_ShowString(100, 140, (u8*)"BACKWARD", YELLOW, RED);
            }
            else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                Motor_TurnRight(); // 왼쪽에 장애물 -> 우회전
                LCD_ShowString(100, 140, (u8*)"RIGHT   ", YELLOW, RED);
            }
            else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                Motor_TurnLeft(); // 오른쪽에 장애물 -> 좌회전
                LCD_ShowString(100, 140, (u8*)"LEFT    ", YELLOW, RED);
            }
            else {
                Motor_Forward();
                LCD_ShowString(100, 140, (u8*)"FORWARD ", WHITE, RED);
            }

            // 3. 소리 재생 (매 루프마다 재생하면 주행이 끊기므로 5번에 1번만 재생)
            sound_tick++;
            if (sound_tick > 5) {
                Play_Reveille();
                sound_tick = 0;
            }
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();
            {
                uint16_t rain_val = (uint16_t)ADC_Value[0];
                sprintf(lcd_buffer, "Rain: %04d", rain_val);
                LCD_ShowString(40, 150, (u8*)lcd_buffer, BLACK, YELLOW);

                if (stability_count < 10) stability_count++;
                else {
                    if (rain_val < 2000) {
                        *p_alarm_state = STATE_ALARM_STOPPED;
                        TIM_Cmd(TIM2, DISABLE);
                    }
                }

                // 빗물 대기 모드에서도 소리가 너무 잦으면 센서 확인이 느려질 수 있음
                sound_tick++;
                if (sound_tick > 10) {
                    Play_Reveille();
                    sound_tick = 0;
                }
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            sprintf(lcd_buffer, "Stop: %d sec", (int)*p_elapsed_seconds);
            LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();

            sensor_timer++;
            if (sensor_timer > 2000) {
                dist_L = Get_Ultrasonic_Dist(1);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(3);
                sensor_timer = 0;

                sprintf(lcd_buffer, "L:%3d", (int)dist_L);
                LCD_ShowString(20, 120, (u8*)lcd_buffer, BLUE, WHITE);

                sprintf(lcd_buffer, "C:%3d", (int)dist_C);
                LCD_ShowString(110, 120, (u8*)lcd_buffer, RED, WHITE);

                sprintf(lcd_buffer, "R:%3d", (int)dist_R);
                LCD_ShowString(200, 120, (u8*)lcd_buffer, BLUE, WHITE);

                LCD_ShowString(60, 150, (u8*)"[Waiting...]", BLACK, WHITE);
            }
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
    Motor_Stop();

    Sensor_Mode_Reset();

    // 초기화면 갱신
    LCD_Clear(WHITE);
    LCD_ShowString(40, 50, (u8*)"[IDLE MODE]", BLACK, WHITE);
    LCD_ShowString(40, 80, (u8*)"Sensor Check", BLUE, WHITE);
}
