#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "inc/hw_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t ADC_Value[1];

// --- Global Variables ---
static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// --- 기상나팔 설정 ---
#define NOTE_G  3000
#define NOTE_C  2250
#define NOTE_E  1800

uint16_t reveille_notes[] = { NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G };
uint32_t reveille_beats[] = { 5, 5, 5, 5, 10 };

// --- Helper Functions ---
void Time_Format(uint32_t total_seconds, char* buffer) {
    uint32_t h = total_seconds / 3600;
    uint32_t m = (total_seconds % 3600) / 60;
    uint32_t s = total_seconds % 60;
    sprintf(buffer, "%02d:%02d:%02d", (int)h, (int)m, (int)s);
}

static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    for (uint32_t i = 0; i < duration; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        if (*p_alarm_state != STATE_ALARM_ACTIVE && *p_alarm_state != STATE_WAIT_FOR_RAIN) return;
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
    TIM_Cmd(TIM2, DISABLE);
}

// 초기화 (IDLE 상태)
void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_countdown_seconds = 0;
    *p_elapsed_seconds = 0;
    TIM_Cmd(TIM2, DISABLE);
    Motor_Stop();
    LCD_Clear(WHITE);
    // [IDLE 화면] 블루투스 연결 대기
    LCD_ShowString(20, 50, (u8*)"Wait BT Connect...", BLACK, WHITE);
}

// [NEW] 블루투스 연결 확인 시 호출 (숫자 대기 화면)
void Alarm_Set_BT_Connected(void) {
    if (*p_alarm_state == STATE_IDLE) {
        *p_alarm_state = STATE_BT_CONNECTED;
        LCD_Clear(WHITE);

        // [요청 화면]
        LCD_ShowString(20, 50, (u8*)"Waiting for", BLACK, WHITE);
        LCD_ShowString(20, 70, (u8*)"alarm setting.", BLACK, WHITE);
        LCD_ShowString(20, 110, (u8*)"send number(second)", BLACK, WHITE);
        LCD_ShowString(20, 130, (u8*)"by bluetooth?", BLACK, WHITE);

        // PC/앱으로 메시지 전송
        USART2_SendString("\r\nBluetooth Connected! Waiting for alarm setting.\r\n");
    }
}

// 숫자 수신 시 호출 (카운트다운 시작)
void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        Sensor_Mode_Reset();
        Motor_Stop();
        LCD_Clear(WHITE);
        // [요청] Countdown 제목 표시
        LCD_ShowString(40, 50, (u8*)"[Countdown]", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

AlarmState Alarm_GetState(void) { return *p_alarm_state; }
uint32_t Alarm_GetElapsedSeconds(void) { return *p_elapsed_seconds; }

// --- Main Process ---
void Alarm_Process(void) {
    char lcd_buffer[30];
    char time_str[20];
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;
    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    static uint32_t last_dist_L = 999, last_dist_C = 999, last_dist_R = 999;
    static char last_action[20] = "";
    char current_action[20];
    static uint32_t sensor_timer = 0;
    const uint32_t OBS_THRESHOLD = 25;

    // 상태 변경 감지 시 화면 초기화 (중복 그리기 방지)
    if (last_state != *p_alarm_state) {
        if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
            LCD_Clear(YELLOW);
            LCD_ShowString(40, 50, (u8*)"WAIT RAIN...", BLACK, YELLOW);
            stability_count = 0; Motor_Stop();
        } else if (*p_alarm_state == STATE_ALARM_STOPPED) {
            LCD_Clear(WHITE); Motor_Stop();
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
             LCD_Clear(RED);
             LCD_ShowString(40, 20, (u8*)"RUNAWAY ALARM!", WHITE, RED);
             last_dist_L = 999; last_action[0] = '\0';
        } else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop(); LCD_Clear(WHITE);
             LCD_ShowString(20, 50, (u8*)"Wait BT Connect...", BLACK, WHITE);
        }
        // STATE_BT_CONNECTED는 함수 내에서 이미 그림

        last_state = *p_alarm_state;
    }

    switch (*p_alarm_state) {
        case STATE_IDLE:
            Motor_Stop();
            // [요청 1] 연결 전: 초음파 센서값 아래로(세로로) 3개 표시
            sensor_timer++;
            if (sensor_timer > 500) {
                dist_L = Get_Ultrasonic_Dist(1);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(3);
                sensor_timer = 0;

                // 세로 배치 (Y좌표를 다르게 설정)
                sprintf(lcd_buffer, "L: %3d cm", (int)dist_L);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);

                sprintf(lcd_buffer, "C: %3d cm", (int)dist_C);
                LCD_ShowString(40, 130, (u8*)lcd_buffer, RED, WHITE);

                sprintf(lcd_buffer, "R: %3d cm", (int)dist_R);
                LCD_ShowString(40, 160, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;

        case STATE_BT_CONNECTED:
            // [요청 2] 숫자 대기 중: 아무것도 안 하고 대기
            // 센서값 숨김, 카운트다운 숨김
            Motor_Stop();
            break;

        case STATE_COUNTDOWN:
            // [요청 3] 카운트다운 표시: reminder 00:00:00
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                Time_Format(last_sec, time_str);
                sprintf(lcd_buffer, "reminder %s", time_str);
                LCD_ShowString(20, 100, (u8*)lcd_buffer, BLACK, WHITE);
            }
            // 센서값 표시 안 함
            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            if(dist_L != last_dist_L || dist_C != last_dist_C || dist_R != last_dist_R) {
                sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
                LCD_ShowString(20, 80, (u8*)lcd_buffer, YELLOW, RED);
                last_dist_L = dist_L; last_dist_C = dist_C; last_dist_R = dist_R;
            }

            // 모터 회피 로직
            if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Go Back");
                Motor_Backward();
            } else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Turn R ");
                Motor_TurnRight();
            } else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Turn L ");
                Motor_TurnLeft();
            } else {
                sprintf(current_action, "Action: Forward");
                Motor_Forward();
            }

            if (strcmp(current_action, last_action) != 0) {
                LCD_ShowString(20, 140, (u8*)current_action, WHITE, RED);
                strcpy(last_action, current_action);
            }
            Play_Reveille();
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();
            {
                uint16_t rain_val = (uint16_t)ADC_Value[0];
                static uint16_t last_rain_val = 9999;

                if (abs((int)rain_val - (int)last_rain_val) > 50) {
                    sprintf(lcd_buffer, "Rain Sensor: %04d", rain_val);
                    LCD_ShowString(20, 150, (u8*)lcd_buffer, BLACK, YELLOW);
                    last_rain_val = rain_val;
                }

                if (stability_count < 100) stability_count++;
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
             Motor_Stop();
             if (last_sec != *p_elapsed_seconds) {
                last_sec = *p_elapsed_seconds;
                Time_Format(*p_elapsed_seconds, time_str);
                sprintf(lcd_buffer, "Total: %s", time_str);
                LCD_ShowString(20, 100, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;
    }
}
