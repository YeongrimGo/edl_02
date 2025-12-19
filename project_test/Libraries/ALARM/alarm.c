#include "alarm.h"
#include "lcd.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"
#include "misc.h"
#include <stdio.h>

// This file's internal variables
static volatile AlarmState alarm_state = STATE_IDLE;
static volatile uint32_t countdown_seconds = 0;
static volatile uint32_t elapsed_seconds = 0;

// Public global variables for access from ISRs in other files
volatile uint32_t* p_countdown_seconds = &countdown_seconds;
volatile uint32_t* p_elapsed_seconds = &elapsed_seconds;
volatile AlarmState* p_alarm_state = &alarm_state;

// Internal function prototypes
static void TIM_Configure_Alarm(void);

// --- Configuration Functions ---

// Simplified to only configure the timer, assuming RCC and GPIO are handled in main hardware config.
static void TIM_Configure_Alarm(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    // 1-second interrupt setup
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period = 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

// --- Public Functions ---

void Alarm_Init(void) {
    TIM_Configure_Alarm();
}

void Alarm_Start(uint16_t minutes) {
    if (minutes > 0) {
        *p_countdown_seconds = minutes * 60;
        *p_alarm_state = STATE_COUNTDOWN;
        *p_elapsed_seconds = 0;
        
        LCD_Clear(WHITE);
        LCD_ShowString(40, 100, (u8*)"Alarm Set", BLUE, WHITE);
        
        // Start timer
        TIM_Cmd(TIM2, ENABLE);
    }
}
void Buzzer_Delay(uint32_t count) {
    while(count--) {
        __NOP(); 
    }
}

void Alarm_Process(void) {
    char lcd_buffer[30];
    static int32_t last_displayed_second = -1;
    static uint8_t buzzer_toggle = 0; 

    switch (*p_alarm_state) {
        case STATE_COUNTDOWN:
            {
                uint32_t remaining_total_seconds = *p_countdown_seconds;
                uint16_t rem_minutes = remaining_total_seconds / 60;
                uint16_t rem_seconds = remaining_total_seconds % 60;

                // Update screen every second to show countdown
                if (last_displayed_second != rem_seconds) {
                    last_displayed_second = rem_seconds;
                    sprintf(lcd_buffer, "Time: %02d min %02d sec", rem_minutes, rem_seconds);
                    LCD_ShowString(40, 130, (u8*)lcd_buffer, BLUE, WHITE);
                }
            }
            
            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            break;

        case STATE_ALARM_ACTIVE:
            // Change screen only once when alarm starts ringing
            if (*p_elapsed_seconds == 1) {
                LCD_Clear(RED);
                LCD_ShowString(40, 100, (u8*)"WAKE UP!", WHITE, RED);
            }
            if (buzzer_toggle) {
                GPIO_SetBits(GPIOB, GPIO_Pin_0);
                buzzer_toggle = 0;
            } else {
                GPIO_ResetBits(GPIOB, GPIO_Pin_0);
                buzzer_toggle = 1;
            }
            // 미세한 지연을 주어 주파수(음 높낮이)를 조절할 수 있습니다.
            Buzzer_Delay(2000); 
            break;


        case STATE_ALARM_STOPPED:
           GPIO_ResetBits(GPIOB, GPIO_Pin_0);// 부저 OFF (High)
            {
                uint16_t minutes = *p_elapsed_seconds / 60;
                uint16_t seconds = *p_elapsed_seconds % 60;
                LCD_Clear(WHITE);
                sprintf(lcd_buffer, "Elapsed: %d min %d sec", minutes, seconds);
                LCD_ShowString(40, 100, (u8*)lcd_buffer, BLUE, WHITE);
                // State reset is handled by the main loop
            }
            break;

        case STATE_IDLE:
            // No special action in idle state
         GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            break;
    }
}

// --- External Getter/Setter Functions ---

AlarmState Alarm_GetState(void) {
    return *p_alarm_state;
}

uint32_t Alarm_GetElapsedSeconds(void) {
    return *p_elapsed_seconds;
}

void Alarm_Reset(void) {
    *p_alarm_state = STATE_IDLE;
    *p_elapsed_seconds = 0;
    *p_countdown_seconds = 0;
    
    // Stop the timer if it was running
    TIM_Cmd(TIM2, DISABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 확실히 끔

    // Optionally, clear the screen for the next alarm
    LCD_Clear(WHITE);
    LCD_ShowString(40, 100, (u8*)"Alarm Idle", BLUE, WHITE);
}
