#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "inc/hw_config.h" // 초음파 함수 사용을 위해 필수
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

        if (*p_alarm_state == STATE_ALARM_STOPPED || *p_alarm_state == STATE_IDLE) return;
    }
}

void Play_Reveille(void) {
    static int note_idx = 0;
    int num_notes = sizeof(reveille_notes) / sizeof(reveille_notes[0]);

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

        Sensor_Mode_Reset();

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

    // 초음파 센서용 변수
    static uint32_t dist1 = 0, dist2 = 0, dist3 = 0;
    static uint32_t sensor_timer = 0;

    if (last_state != *p_alarm_state) {
        switch (*p_alarm_state) {
            case STATE_WAIT_FOR_RAIN:
                LCD_Clear(YELLOW);
                LCD_ShowString(40, 50, (u8*)"WAITING RAIN...", BLACK, YELLOW);
                LCD_ShowString(40, 100, (u8*)"ANALOG (PA1)", BLACK, YELLOW);
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
            {
                uint16_t rain_val = (uint16_t)ADC_Value[0];
                sprintf(lcd_buffer, "Rain ADC: %04d", rain_val);
                LCD_ShowString(40, 150, (u8*)lcd_buffer, BLACK, YELLOW);

                // [안정화 딜레이]
                if (stability_count < 10) {
                    stability_count++;
                }
                else {
                    if (rain_val < 2000) {
                        *p_alarm_state = STATE_ALARM_STOPPED;
                        TIM_Cmd(TIM2, DISABLE);
                        USART2_SendString("\r\nRain Detected (Analog)! Alarm Stopped.\r\n");
                    }
                }
                Play_Reveille();
            }
            break;

        case STATE_ALARM_STOPPED:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);
            sprintf(lcd_buffer, "Stopped: %d sec", (int)*p_elapsed_seconds);
            LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
            break;

        case STATE_IDLE:
            GPIO_SetBits(GPIOB, GPIO_Pin_0);

            // [추가됨] 초음파 센서 표시 로직
            sensor_timer++;
            if (sensor_timer > 10000) { // LCD 깜빡임 방지용 딜레이
                dist1 = Get_Ultrasonic_Dist(1); // 왼쪽
                dist2 = Get_Ultrasonic_Dist(2); // 정면
                dist3 = Get_Ultrasonic_Dist(3); // 오른쪽
                sensor_timer = 0;

                LCD_ShowString(80, 120, (u8*)"[Ultrasonic]", BLACK, WHITE);

                sprintf(lcd_buffer, "L:%3d", (int)dist1);
                LCD_ShowString(10, 140, (u8*)lcd_buffer, BLUE, WHITE);

                sprintf(lcd_buffer, "C:%3d", (int)dist2);
                LCD_ShowString(110, 140, (u8*)lcd_buffer, RED, WHITE);

                sprintf(lcd_buffer, "R:%3d", (int)dist3);
                LCD_ShowString(210, 140, (u8*)lcd_buffer, BLUE, WHITE);
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

    Sensor_Mode_Reset();

    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}
