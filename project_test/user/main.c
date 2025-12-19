#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "alarm.h"
#include "inc/hw_config.h"
#include <stdio.h>

volatile uint32_t ADC_Value[1];

int main(void) {
    SystemInit();

    RCC_Configure();
    GPIO_Configure();
    NVIC_Configure();

    // 부저 PWM 준비 (PC6 + TIM3 remap)
    BUZZER_Configure();

    ADC_Configure();
    DMA_Configure();

    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();

    USART1_Init();
    USART2_Init();

    Alarm_Init();
    Alarm_Reset();

    USART2_SendString("System Ready! Send seconds (e.g. 10)\r\n");

    while (1) {
        Alarm_Process();

        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            char msg[50];
            sprintf(msg, "Take: %d secs\r\n", (int)Alarm_GetElapsedSeconds());
            USART2_SendString(msg);
            Alarm_Reset();
        }
    }
}
