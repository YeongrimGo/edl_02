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

// --- 기상나팔 설정 (반응속도 최적화) ---
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

    // 1초 타이머 설정 (SystemCoreClock / 7200 / 10000 = 1Hz)
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, DISABLE);
}

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_countdown_seconds = 0;
    *p_elapsed_seconds = 0;
    TIM_Cmd(TIM2, DISABLE);
    Motor_Stop();
    LCD_Clear(WHITE);
    // [요청 1] 블루투스 연결 대기 문구
    LCD_ShowString(20, 50, (u8*)"Wait BT Connect...", BLACK, WHITE);
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        Sensor_Mode_Reset();
        Motor_Stop();

        LCD_Clear(WHITE);
        // [요청 3] Countdown 제목 표시
        LCD_ShowString(40, 50, (u8*)"[Countdown]", BLUE, WHITE);

        TIM_Cmd(TIM2, ENABLE);
    }
}

AlarmState Alarm_GetState(void) {
    return *p_alarm_state;
}

uint32_t Alarm_GetElapsedSeconds(void) {
    return *p_elapsed_seconds;
}

// --- Main Alarm Process (Countdown 센서 출력 제거 확인) ---
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

    // 1. 상태 변경 시 초기화 및 화면 클리어
    if (last_state != *p_alarm_state) {
        if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
            LCD_Clear(YELLOW);
            LCD_ShowString(10, 40, (u8*)"WAITING FOR WATER...", BLACK, YELLOW);
            LCD_ShowString(10, 70, (u8*)"GO TO BATHROOM!!", RED, YELLOW);
            LCD_ShowString(10, 100, (u8*)"DON'T SLEEP AGAIN!!", RED, YELLOW);
            stability_count = 0;
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_ALARM_STOPPED) {
            LCD_Clear(WHITE);
            Motor_Stop();
        }
        else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
             LCD_Clear(RED);
             LCD_ShowString(40, 20, (u8*)"RUNAWAY ALARM!", WHITE, RED);
             LCD_ShowString(40, 50, (u8*)"Catch the Alarm!", WHITE, RED);
             LCD_ShowString(20, 80, (u8*)"Touch the Sensor!", YELLOW, RED);

             last_dist_L = 999;
             last_action[0] = '\0';
        }
        else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop();
             LCD_Clear(WHITE);
             LCD_ShowString(20, 50, (u8*)"Wait BT Connect...", BLACK, WHITE);
        }
        // STATE_COUNTDOWN으로 진입 시의 LCD_Clear는 Alarm_Start() 함수에서 처리된다고 가정합니다.

        last_state = *p_alarm_state;
    }

    // 2. 상태별 동작
    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            // [확인] 이 상태에서는 초음파 센서 값을 읽거나 출력하지 않습니다.
            // 오직 시간만 갱신합니다.
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                Time_Format(last_sec, time_str);
                sprintf(lcd_buffer, "reminder %s", time_str);
                LCD_ShowString(20, 100, (u8*)lcd_buffer, BLACK, WHITE);
            }
            GPIO_ResetBits(GPIOB, GPIO_Pin_0); // 부저 끔
            Motor_Stop(); // 모터 정지
            break;

        case STATE_ALARM_ACTIVE:
            // 센서 값은 내부 로직용으로만 읽고, 화면에는 출력하지 않음
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            // 모터 제어 (장애물 회피)
            if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Go Back");
                Motor_Backward();
            }
            else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Turn R ");
                Motor_TurnRight();
            }
            else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                sprintf(current_action, "Action: Turn L ");
                Motor_TurnLeft();
            }
            else {
                sprintf(current_action, "Action: Forward");
                Motor_Forward();
            }

            // Action 상태 표시
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
                    sprintf(lcd_buffer, "Water Sensor: %04d", rain_val);
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
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();

            if (last_sec != *p_elapsed_seconds) {
                last_sec = *p_elapsed_seconds;
                Time_Format(*p_elapsed_seconds, time_str);
                sprintf(lcd_buffer, "Total: %s", time_str);
                LCD_ShowString(20, 100, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();

            // IDLE 상태에서만 센서 값을 출력합니다.
            sensor_timer++;
            if (sensor_timer > 500) {
                dist_L = Get_Ultrasonic_Dist(1);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(3);
                sensor_timer = 0;

                sprintf(lcd_buffer, "L: %3d cm", (int)dist_L);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);

                sprintf(lcd_buffer, "C: %3d cm", (int)dist_C);
                LCD_ShowString(40, 125, (u8*)lcd_buffer, RED, WHITE);

                sprintf(lcd_buffer, "R: %3d cm", (int)dist_R);
                LCD_ShowString(40, 150, (u8*)lcd_buffer, BLUE, WHITE);
            }
            break;
    }
}
