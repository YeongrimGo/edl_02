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

// --- 기상나팔 (빠른 반응을 위해 비트 길이를 조금 줄임) ---
#define NOTE_G  3000
#define NOTE_C  2250
#define NOTE_E  1800

uint16_t reveille_notes[] = {
    NOTE_G, NOTE_C, NOTE_E, NOTE_C, NOTE_G
};
uint32_t reveille_beats[] = {
    50, 50, 50, 50, 100 // 비트 길이를 줄여 센서 업데이트 주기를 확보
};

// [수정] 00분 00초 포맷 (시간 제외)
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

        // 알람이 꺼지면 즉시 리턴
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

    // 자율주행 관련 변수
    static uint32_t last_decision_time = 0;
    static uint32_t dist_L = 0, dist_C = 0, dist_R = 0;
    const uint32_t OBS_THRESHOLD = 25;

    // 상태 변경 시 화면 초기화 및 변수 리셋
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
             last_decision_time = 0;
        }
        else if (*p_alarm_state == STATE_IDLE) {
             Motor_Stop();
             LCD_Clear(WHITE);
             LCD_ShowString(40, 50, (u8*)"[IDLE MODE]", BLACK, WHITE);
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
            Motor_Stop();
            break;

        case STATE_ALARM_ACTIVE:
            // 1. 화면 표시 (항상 표시)
            LCD_ShowString(40, 20, (u8*)"RUNAWAY ALARM!", WHITE, RED);

            // 경과 시간 표시 (00분 00초)
            Time_Format(*p_elapsed_seconds, time_str);
            sprintf(lcd_buffer, "Time: %s", time_str);
            LCD_ShowString(40, 200, (u8*)lcd_buffer, YELLOW, RED);

            // 2. 센서값 실시간 업데이트 (지연 없이 매번 읽음)
            // [수정] 속도 개선을 위해 불필요한 for 루프 제거
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
            LCD_ShowString(20, 80, (u8*)lcd_buffer, YELLOW, RED);

            // 3. 자율주행 로직 (2초 주기)
            // 현재 시간이 짝수 초(0, 2, 4...)이고, 아직 이번 초에 결정을 안 내렸다면 실행
            if ((*p_elapsed_seconds % 2 == 0) && (*p_elapsed_seconds != last_decision_time)) {

                // (1) 바퀴 정지
                Motor_Stop();

                // (2) 아주 잠깐 대기 (센서 안정화 필요 시, 없어도 됨)
                // for(volatile int k=0; k<1000; k++);

                // (3) 방향 결정 (이미 위에서 읽은 최신 dist 값 사용)
                if (dist_C > 0 && dist_C < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obs: Front     ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Act: Backward  ", WHITE, RED);
                    Motor_Backward();
                }
                else if (dist_L > 0 && dist_L < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obs: Left      ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Act: Turn Right", WHITE, RED);
                    Motor_TurnRight();
                }
                else if (dist_R > 0 && dist_R < OBS_THRESHOLD) {
                    LCD_ShowString(20, 110, (u8*)"Obs: Right     ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Act: Turn Left ", WHITE, RED);
                    Motor_TurnLeft();
                }
                else {
                    LCD_ShowString(20, 110, (u8*)"Path Clear     ", WHITE, RED);
                    LCD_ShowString(20, 140, (u8*)"Act: Forward   ", WHITE, RED);
                    Motor_Forward();
                }

                last_decision_time = *p_elapsed_seconds; // 이번 주기는 처리 완료
            }

            // 소리 재생 (루프를 짧게 끊어쳐서 센서 업데이트 자주 함)
            Play_Reveille();
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();

            // 경과 시간 표시
            Time_Format(*p_elapsed_seconds, time_str);
            sprintf(lcd_buffer, "Time: %s", time_str);
            LCD_ShowString(40, 110, (u8*)lcd_buffer, BLACK, YELLOW);

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

            // IDLE 상태에서도 센서 빠르게 확인
            dist_L = Get_Ultrasonic_Dist(1);
            dist_C = Get_Ultrasonic_Dist(2);
            dist_R = Get_Ultrasonic_Dist(3);

            sprintf(lcd_buffer, "L:%3d", (int)dist_L);
            LCD_ShowString(20, 120, (u8*)lcd_buffer, BLUE, WHITE);

            sprintf(lcd_buffer, "C:%3d", (int)dist_C);
            LCD_ShowString(110, 120, (u8*)lcd_buffer, RED, WHITE);

            sprintf(lcd_buffer, "R:%3d", (int)dist_R);
            LCD_ShowString(200, 120, (u8*)lcd_buffer, BLUE, WHITE);

            LCD_ShowString(60, 150, (u8*)"[Waiting...]", BLACK, WHITE);

            // 약간의 딜레이만 줌 (화면 깜빡임 방지용)
            for(volatile int i=0; i<50000; i++);
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
