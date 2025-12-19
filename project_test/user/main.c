#include "stm32f10x.h"
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
    
    ADC_Configure();
    DMA_Configure();
    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();
    
    USART1_Init(); 
    USART2_Init(); 
    
    Alarm_Init();
    Alarm_Reset(); 

    USART2_SendString("System Ready!\r\n");

    while (1) {
        Alarm_Process();
        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            char msg[50];
            sprintf(msg, "Time: %d secs\r\n", (int)Alarm_GetElapsedSeconds());
            USART2_SendString(msg);
            
            // 결과 표시 후 리셋 대기
            for(volatile int i=0; i<5000000; i++); 
            Alarm_Reset();
        }
    }
}