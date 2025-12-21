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

// --- 기상나팔 ---
#define NOTE_G  3000
#define NOTE_C  2250
#define NOTE_E  1800

uint16_t reveille_notes[] = {
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G
};
uint32_t reveille_beats[] = {
    100, 100, 100, 100, 200
};

// MM:SS 형식 변환 함수
void Time_Format(uint32_t total_seconds, char* buffer) {
    uint32_t m = total_seconds / 60;
    uint32_t s = total_seconds % 60;
    sprintf(buffer, "%02d:%02d", (int)m, (int)s);
}

static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    // 소리를 내는 동안에도 루프를 너무 오래 잡고 있지 않도록 주의
    // 여기서는 duration만큼 루프를 돌기 때문에 소리가 나는 동안은 센서 업데이트가 잠깐 멈출 수 있음
    // 하지만 duration이 짧다면 크게 문제되지 않음
    for (uint32_t i = 0; i < duration; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);

        // 상태가 바뀌면 즉시 종료
        if (*p_alarm_state == STATE_ALARM_STOPPED || *p_alarm_state == STATE_IDLE || *p_alarm_state == STATE_WAIT_BLUETOOTH) return;
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

    if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
        // 비트(duration)를 조금 줄여서 센서 반응 속도를 높일 수 있음 (현재 유지)
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
        Motor_Stop();

        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

// 간단한 딜레이 함수
void Delay_loop(uint32_t count) {
    for(volatile uint32_t i=0; i<count; i++);
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    char time_str[20];
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;

    // 자율주행 관련 변수
    static uint32_t last_decision_time = 0;
    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    const uint32_t OBS_THRESHOLD = 30; // 장애물 감지 거리 (조절 가능)

    // 상태 변경 감지 및 LCD 초기화
    if (last_state != *p_alarm_state) {
        if (*p_alarm_state == STATE_WAIT_BLUETOOTH) {
            LCD_Clear(WHITE);
            LCD_ShowString(20, 80, (u8*)"Waiting for", BLACK, WHITE);
            LCD_ShowString(20, 110, (u8*)"Bluetooth", BLACK, WHITE);
            LCD_ShowString(20, 140, (u8*)"Connection...", BLACK, WHITE);
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_IDLE) {
            LCD_Clear(WHITE);
            LCD_ShowString(20, 80, (u8*)"Waiting for", BLACK, WHITE);
            LCD_ShowString(20, 110, (u8*)"Alarm Setting", BLACK, WHITE);
            LCD_ShowString(20, 140, (u8*)"Send Time (sec)", BLUE, WHITE);
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
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
             last_decision_time = 0;
             // 초기 시작 시 정지
             Motor_Stop();
        }
        last_state = *p_alarm_state;
    }

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                Time_Format(last_sec, time_str);
                sprintf(lcd_buffer, "Rem: %s", time_str);
                LCD_ShowString(20, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            LCD_ShowString(40, 20, (u8*)"RUNAWAY ALARM!", WHITE, RED);

            // 1. 시간 표시 (MM:SS)
            if (last_sec != *p_elapsed_seconds) {
                last_sec = *p_elapsed_seconds;
                Time_Format(*p_elapsed_seconds, time_str);
                sprintf(lcd_buffer, "Time: %s", time_str);
                LCD_ShowString(20, 250, (u8*)lcd_buffer, WHITE, RED);
            }

            // 2. [수정] 센서 값 실시간 측정 및 LCD 표시
            //    조건문 없이 항상 실행하여 반응속도를 높임
            dist_L = Get_Ultrasonic_Dist(1);
            // 너무 빠른 연속 호출 방지를 위해 아주 짧은 딜레이만 줌 (기존보다 훨씬 짧게)
            Delay_loop(1000);
            dist_C = Get_Ultrasonic_Dist(2);
            Delay_loop(1000);
            dist_R = Get_Ultrasonic_Dist(3);

            // 실시간 값 LCD 출력
            sprintf(lcd_buffer, "L:%2d C:%2d R:%2d   ", (int)dist_L, (int)dist_C, (int)dist_R);
            LCD_ShowString(20, 80, (u8*)lcd_buffer, YELLOW, RED);


            // 3. [수정] 2초 주기 이동 방향 결정 로직
            //    2초가 지났고, 아직 처리를 안 했다면 진입
            if ((*p_elapsed_seconds % 2 == 0) && (*p_elapsed_seconds != last_decision_time)) {

                last_decision_time = *p_elapsed_seconds;

                // (A) 일단 멈춤
                Motor_Stop();
                LCD_ShowString(20, 140, (u8*)"Thinking...    ", WHITE, RED);

                // (B) 멈춘 상태에서 잠깐 대기 (센서값 확인 & 멈춤 동작 시각화)
                // 약 0.2~0.3초 정도 대기
                Delay_loop(2000000);

                // (C) 최신 센서 값(위에서 읽은 값)을 보고 방향 결정 및 이동 시작
                if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obstacle: Front", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Action: Go Back", WHITE, RED);
                    Motor_Backward();
                }
                else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obstacle: Left ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Action: Turn R ", WHITE, RED);
                    Motor_TurnRight();
                }
                else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obstacle: Right", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Action: Turn L ", WHITE, RED);
                    Motor_TurnLeft();
                }
                else {
                    LCD_ShowString(20, 110, (u8*)"Path Clear     ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Action: Forward", WHITE, RED);
                    Motor_Forward();
                }
            }

            // 이동 중에도 소리는 계속 남
            Play_Reveille();
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();

            if (last_sec != *p_elapsed_seconds) {
                last_sec = *p_elapsed_seconds;
                Time_Format(*p_elapsed_seconds, time_str);
                sprintf(lcd_buffer, "Time: %s", time_str);
                LCD_ShowString(20, 200, (u8*)lcd_buffer, BLACK, YELLOW);
            }

            {
                uint16_t rain_val = (uint16_t)ADC_Value[0];
                sprintf(lcd_buffer, "Rain Sensor: %04d", rain_val);
                LCD_ShowString(20, 150, (u8*)lcd_buffer, BLACK, YELLOW);

                if (stability_count < 10) stability_count++;
                else {
                    if (rain_val < 2000) {
                        *p_alarm_state = STATE_ALARM_STOPPED;
                        TIM_Cmd(TIM2, DISABLE);
                    }
                }
            }
            Play_Reveille();
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();

            Time_Format(*p_elapsed_seconds, time_str);
            sprintf(lcd_buffer, "Total: %s", time_str);
            LCD_ShowString(20, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_WAIT_BLUETOOTH:
        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
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
}
