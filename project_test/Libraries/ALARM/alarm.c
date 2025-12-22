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
// [수정] 박자를 짧게 줄여 부저가 울리는 동안 센서가 멈추는 시간을 최소화
uint32_t reveille_beats[] = { 5, 5, 5, 5, 10 };

// --- Helper Functions ---

void Time_Format(uint32_t total_seconds, char* buffer) {
    uint32_t h = total_seconds / 3600;
    uint32_t m = (total_seconds % 3600) / 60;
    uint32_t s = total_seconds % 60;
    sprintf(buffer, "%02d:%02d:%02d", (int)h, (int)m, (int)s);
}

// [수정] 부저 소리 출력 (Loop 중단 조건 추가)
static void Buzzer_Sound(uint16_t pitch, uint32_t duration) {
    for (uint32_t i = 0; i < duration; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        for (volatile int d = 0; d < pitch; d++);

        // [중요] 알람 상태가 아니거나 비 대기 상태가 아니면 소리를 즉시 멈추고 복귀
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
    LCD_ShowString(40, 50, (u8*)"[IDLE MODE]", BLACK, WHITE);
}

void Alarm_Start(uint16_t seconds) {
    if (seconds > 0) {
        *p_countdown_seconds = seconds;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;

        // hw_config.c 등에 정의된 센서 초기화 함수 호출
        Sensor_Mode_Reset();
        Motor_Stop();

        LCD_Clear(WHITE);
        LCD_ShowString(40, 50, (u8*)"COUNTDOWN...", RED, WHITE);

        TIM_Cmd(TIM2, ENABLE);
    }
}

AlarmState Alarm_GetState(void) {
    return *p_alarm_state;
}

uint32_t Alarm_GetElapsedSeconds(void) {
    return *p_elapsed_seconds;
}

// --- Main Alarm Process (최적화 적용) ---
void Alarm_Process(void) {
    char lcd_buffer[30];
    char time_str[20];

    // 상태 추적용 정적 변수 (이전 값과 비교하기 위함)
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;

    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    static uint32_t last_dist_L = 999, last_dist_C = 999, last_dist_R = 999;

    static char last_action[20] = "";
    char current_action[20];

    static uint32_t sensor_timer = 0;
    const uint32_t OBS_THRESHOLD = 25; // 장애물 감지 거리 (cm)

    // 1. 상태가 변경되었을 때 한 번만 실행되는 초기화 로직 (화면 깜빡임 방지)
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
             LCD_ShowString(40, 20, (u8*)"RUNAWAY ALARM!", WHITE, RED);
             // 활성 상태 진입 시 값 초기화
             last_dist_L = 999;
             last_action[0] = '\0';
        }
        else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop();
             LCD_Clear(WHITE);
             LCD_ShowString(40, 50, (u8*)"[IDLE MODE]", BLACK, WHITE);
        }
        last_state = *p_alarm_state;
    }

    // 2. 상태별 반복 동작
    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            // 초 단위 시간이 바뀔 때만 LCD 갱신
            if (last_sec != *p_countdown_seconds) {
                last_sec = *p_countdown_seconds;
                Time_Format(last_sec, time_str);
                sprintf(lcd_buffer, "Rem: %s", time_str);
                LCD_ShowString(20, 100, (u8*)lcd_buffer, BLUE, WHITE);
            }

            // 카운트다운 중에도 센서값은 확인하되, 값이 변할 때만 LCD 출력
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            if(dist_L != last_dist_L || dist_C != last_dist_C || dist_R != last_dist_R) {
                 sprintf(lcd_buffer, "SENS: %2d %2d %2d", (int)dist_L, (int)dist_C, (int)dist_R);
                 LCD_ShowString(20, 140, (u8*)lcd_buffer, BLACK, WHITE);
                 last_dist_L = dist_L; last_dist_C = dist_C; last_dist_R = dist_R;
            }

            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            // (1) 센서 값 읽기 (매 루프 실행)
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            // (2) 센서 값 LCD 출력 (값이 변했을 때만 실행 -> 속도 향상 핵심)
            if(dist_L != last_dist_L || dist_C != last_dist_C || dist_R != last_dist_R) {
                sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
                LCD_ShowString(20, 80, (u8*)lcd_buffer, YELLOW, RED);
                last_dist_L = dist_L; last_dist_C = dist_C; last_dist_R = dist_R;
            }

            // (3) 모터 제어 로직 (장애물 회피)
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

            // (4) 동작 상태 LCD 출력 (동작 텍스트가 바뀔 때만 실행)
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

                // 빗물 센서 값 변화폭이 클 때만 LCD 갱신
                if (abs((int)rain_val - (int)last_rain_val) > 50) {
                    sprintf(lcd_buffer, "Rain Sensor: %04d", rain_val);
                    LCD_ShowString(20, 150, (u8*)lcd_buffer, BLACK, YELLOW);
                    last_rain_val = rain_val;
                }

                if (stability_count < 100) stability_count++;
                else {
                    if (rain_val < 2000) { // 빗물 감지 임계값
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

            // 1초마다 시간 갱신
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

            // IDLE 상태에서도 주기적으로 센서 확인 (테스트용)
            // 딜레이 카운트(sensor_timer)를 줄여서 반응성 향상 (1000 -> 500)
            sensor_timer++;
            if (sensor_timer > 500) {
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
            }
            break;
    }
}
