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

        // 알람 상태가 아니면 소리 즉시 중단
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

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;

    // 초음파 센서 변수
    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    static uint32_t sensor_timer = 0;

    // 장애물 인식 거리 (단위: cm)
    const uint32_t OBS_THRESHOLD = 25;

    // 상태 변경 시 화면 초기화
    if (last_state != *p_alarm_state) {
        if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
            LCD_Clear(YELLOW);
            LCD_ShowString(40, 50, (u8*)"WAIT RAIN...", BLACK, YELLOW);
            stability_count = 0;
            Motor_Stop(); // 비 대기 중엔 정지
        }
        else if (*p_alarm_state == STATE_ALARM_STOPPED) {
            LCD_Clear(WHITE);
            Motor_Stop(); // 알람 종료 시 정지
        }
        else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
             LCD_Clear(RED); // 알람 울릴 때 빨간 화면
        }
        else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop(); // 대기 상태 정지
             LCD_Clear(WHITE);
             LCD_ShowString(40, 100, (u8*)"System Ready", BLUE, WHITE);
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
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 끔
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            // 1. 화면 표시 & 소리 재생
            LCD_ShowString(40, 80, (u8*)"RUNAWAY ALARM!", WHITE, RED);
            Play_Reveille(); // 소리 재생 (딜레이 발생함)

            // 2. [자율주행 로직] - 소리 재생 사이사이에 실행됨
            // Play_Reveille 함수 자체가 딜레이 역할을 하므로 별도의 타이머 없이 매번 체크
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            // 거리 정보 표시 (디버깅용)
            sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
            LCD_ShowString(20, 110, (u8*)lcd_buffer, YELLOW, RED);

            // --- 회피 알고리즘 ---
            // A. 정면에 장애물 -> 후진
            if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                Motor_Backward();
                LCD_ShowString(100, 140, (u8*)"BACKWARD", YELLOW, RED);
            }
            // B. 왼쪽에 장애물 -> 우회전
            else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                Motor_TurnRight();
                LCD_ShowString(100, 140, (u8*)"RIGHT   ", YELLOW, RED);
            }
            // C. 오른쪽에 장애물 -> 좌회전
            else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                Motor_TurnLeft();
                LCD_ShowString(100, 140, (u8*)"LEFT    ", YELLOW, RED);
            }
            // D. 장애물 없음 -> 전진
            else {
                Motor_Forward();
                LCD_ShowString(100, 140, (u8*)"FORWARD ", WHITE, RED);
            }
            break;

        case STATE_WAIT_FOR_RAIN:
            {
                // 비 감지 모드에서는 움직이지 않음
                Motor_Stop();

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
                Play_Reveille(); // 소리는 계속 울림
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 끄기
            Motor_Stop(); // 모터 정지
            sprintf(lcd_buffer, "Stop: %d sec", (int)*p_elapsed_seconds);
            LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop(); // [중요] 대기 상태에서는 움직이지 않음

            // 센서 값 확인용 (움직이지는 않고 값만 표시)
            sensor_timer++;
            if (sensor_timer > 5000) {
                dist_L = Get_Ultrasonic_Dist(1);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(3);
                sensor_timer = 0;

                LCD_ShowString(60, 130, (u8*)"[Sensor Test]", BLACK, WHITE);
                sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
                LCD_ShowString(20, 150, (u8*)lcd_buffer, BLUE, WHITE);
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

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"System Ready", BLUE, WHITE);
}
