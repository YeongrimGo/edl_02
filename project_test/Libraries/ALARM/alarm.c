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
        Motor_Stop(); // 알람 설정시 모터 정지

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

    // 센서 값 저장 변수
    uint32_t dist_L = 0, dist_C = 0, dist_R = 0;

    // 거리 기준 (cm)
    const uint32_t OBS_THRESHOLD = 30; // 30cm 이내면 장애물로 인식

    // 상태 변경 시 화면 초기화 (기존과 동일)
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
            LCD_ShowString(40, 50, (u8*)"STEP DRIVE MODE", WHITE, RED);

            // ====================================================
            // STEP 1: 일단 멈춰서 센서 확인 (안전을 위해 정지 후 스캔)
            // ====================================================
            Motor_Stop();
            // 모터 끄고 잠시 대기 (진동 안정화)
            for(volatile int i=0; i<20000; i++);

            // 초음파 측정 (간섭 방지 딜레이 포함)
            dist_L = Get_Ultrasonic_Dist(1);
            for(volatile int i=0; i<5000; i++);

            dist_C = Get_Ultrasonic_Dist(2);
            for(volatile int i=0; i<5000; i++);

            dist_R = Get_Ultrasonic_Dist(3);

            // LCD에 거리 표시
            sprintf(lcd_buffer, "L:%2d C:%2d R:%2d", (int)dist_L, (int)dist_C, (int)dist_R);
            LCD_ShowString(20, 110, (u8*)lcd_buffer, YELLOW, RED);

            // 0값(에러)을 아주 먼 거리(999)로 치환
            if(dist_L == 0) dist_L = 999;
            if(dist_C == 0) dist_C = 999;
            if(dist_R == 0) dist_R = 999;


            // ====================================================
            // STEP 2: 방향 결정 (어디로 갈지 정하기)
            // ====================================================
            if (dist_C < OBS_THRESHOLD) {
                // 정면 막힘 -> 후진 후 넓은 쪽으로
                LCD_ShowString(100, 140, (u8*)"OBSTACLE!   ", YELLOW, RED);
                Motor_Backward(); // 일단 후진 명령 (실행은 아래 소리 재생 동안 지속됨)

                // 후진은 조금 짧게 하고 회전하는 게 좋음 (별도 처리)
                 for(volatile int i=0; i<200000; i++); // 짧게 후진

                 if (dist_L > dist_R) Motor_TurnLeft();
                 else Motor_TurnRight();
            }
            else if (dist_L < OBS_THRESHOLD) {
                LCD_ShowString(100, 140, (u8*)"RIGHT TURN  ", YELLOW, RED);
                Motor_TurnRight();
            }
            else if (dist_R < OBS_THRESHOLD) {
                LCD_ShowString(100, 140, (u8*)"LEFT TURN   ", YELLOW, RED);
                Motor_TurnLeft();
            }
            else {
                // 장애물 없음 -> 전진
                LCD_ShowString(100, 140, (u8*)"GO FORWARD  ", WHITE, RED);
                Motor_Forward();
            }


            // ====================================================
            // STEP 3: 1초 동안 행동 유지 (소리 재생 + 이동)
            // ====================================================
            // Play_Reveille 함수가 실행되는 동안(약 0.6~0.8초)
            // 위에서 설정한 모터 상태가 계속 유지됩니다.
            Play_Reveille();

            // 시간이 조금 부족하면 추가 딜레이로 1초를 맞춤
            for(volatile int i=0; i<100000; i++);

            // ====================================================
            // 루프 끝 -> 다시 위로 올라가서 Motor_Stop() 실행됨
            // ====================================================
            break;

        case STATE_WAIT_FOR_RAIN:
            Motor_Stop();
            // (기존 빗물 감지 코드 유지)
            {
                uint16_t rain_val = (uint16_t)ADC_Value[0];
                sprintf(lcd_buffer, "Rain: %04d", rain_val);
                LCD_ShowString(40, 150, (u8*)lcd_buffer, BLACK, YELLOW);
                if (stability_count < 10) stability_count++;
                else if (rain_val < 2000) {
                    *p_alarm_state = STATE_ALARM_STOPPED;
                    TIM_Cmd(TIM2, DISABLE);
                }

                // 빗물 대기 중에도 소리 재생
                Play_Reveille();
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            Motor_Stop();
            sprintf(lcd_buffer, "Stop: %d sec", (int)*p_elapsed_seconds);
            LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_IDLE:
            // (기존 IDLE 코드 유지)
             GPIO_SetBits(GPIOB, GPIO_Pin_0);
             Motor_Stop();
             // IDLE 상태에서도 센서값 확인용 (필요 시)
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
    Motor_Stop(); // 리셋 시 모터 정지

    Sensor_Mode_Reset();

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"System Ready", BLUE, WHITE);
}
