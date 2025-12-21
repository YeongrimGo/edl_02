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

// [수정] 00시는 빼고 00분 00초 (MM:SS) 형식으로 변경
void Time_Format(uint32_t total_seconds, char* buffer) {
    uint32_t m = total_seconds / 60;
    uint32_t s = total_seconds % 60;
    sprintf(buffer, "%02d:%02d", (int)m, (int)s);
}

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
        Motor_Stop();

        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        TIM_Cmd(TIM2, ENABLE);
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    char time_str[20];
    static int32_t last_sec = -1;
    static AlarmState last_state = STATE_IDLE;
    static uint32_t stability_count = 0;

    // 이동 및 센서 스캔 관련 변수
    static uint32_t last_move_check_time = 0;
    static uint8_t move_decision_needed = 0;
    static uint32_t real_time_sensor_counter = 0; // 실시간 업데이트용 카운터

    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    static uint32_t sensor_timer = 0;

    const uint32_t OBS_THRESHOLD = 25;

    // 상태 변경 감지 및 초기화
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
             last_move_check_time = 0;
             move_decision_needed = 1; // 시작하자마자 판단하도록
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
                Time_Format(last_sec, time_str);
                sprintf(lcd_buffer, "Rem: %s", time_str);
                LCD_ShowString(20, 130, (u8*)lcd_buffer, BLUE, WHITE);
            }
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            // 1. 경과 시간 표시 (00분 00초)
            Time_Format(*p_elapsed_seconds, time_str);
            sprintf(lcd_buffer, "Time: %s", time_str);
            LCD_ShowString(20, 20, (u8*)lcd_buffer, WHITE, RED);

            // 2. [실시간] 센서 값 업데이트 (느리지 않게, 매번 혹은 자주 수행)
            // 소리 재생 사이사이에 호출됨.
            real_time_sensor_counter++;
            // 너무 자주하면 부저가 끊길 수 있으니 약간의 텀을 주되 사용자에게는 '실시간' 처럼 보이게 함
            if (real_time_sensor_counter > 5) {
                // 센서 ID 교차 적용 (3=L, 1=R)
                dist_L = Get_Ultrasonic_Dist(3);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(1);

                sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
                LCD_ShowString(20, 80, (u8*)lcd_buffer, YELLOW, RED);

                real_time_sensor_counter = 0;
            }

            // 3. [2초 주기] 이동 방향 결정 로직
            // 2초마다 트리거
            if ((*p_elapsed_seconds % 2 == 0) && (*p_elapsed_seconds != last_move_check_time)) {
                move_decision_needed = 1;
                last_move_check_time = *p_elapsed_seconds;
            }

            if (move_decision_needed) {
                // (1) 일단 멈춤
                Motor_Stop();
                LCD_ShowString(20, 110, (u8*)"Scan & Decide..", YELLOW, RED);

                // (2) 멈춘 상태에서 정확한 판단을 위해 한 번 더 센서 읽기 (선택사항, 위 실시간 값 써도 됨)
                // 확실하게 하기 위해 읽음
                dist_L = Get_Ultrasonic_Dist(3);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(1);

                // (3) 방향 판단 및 이동 시작
                if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Go Back...     ", WHITE, RED);
                    // hw_config에서 Backward가 '전진'이므로, '후진' 하려면 Forward 호출
                    Motor_Forward();
                }
                else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Turn Right...  ", WHITE, RED);
                    // hw_config에서 TurnLeft가 '우회전'
                    Motor_TurnLeft();
                }
                else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Turn Left...   ", WHITE, RED);
                    // hw_config에서 TurnRight가 '좌회전'
                    Motor_TurnRight();
                }
                else {
                    LCD_ShowString(20, 110, (u8*)"Go Forward...  ", WHITE, RED);
                    // 평소 주행: hw_config에서 Backward가 '전진'
                    Motor_Backward();
                }

                // 결정 완료, 다음 2초가 될 때까지 이 상태 유지
                move_decision_needed = 0;
            }

            Play_Reveille();
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();

            // 경과 시간 표시 추가
            Time_Format(*p_elapsed_seconds, time_str);
            sprintf(lcd_buffer, "Time: %s", time_str);
            LCD_ShowString(20, 20, (u8*)lcd_buffer, BLACK, YELLOW);

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

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();

            sensor_timer++;
            if (sensor_timer > 2000) {
                // ID 교차 적용
                dist_L = Get_Ultrasonic_Dist(3);
                dist_C = Get_Ultrasonic_Dist(2);
                dist_R = Get_Ultrasonic_Dist(1);
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

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"System Ready", BLUE, WHITE);
}
