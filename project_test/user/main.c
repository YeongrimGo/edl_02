#include "stm32f10x.h"
#include "lcd.h"
#include "alarm.h"
#include <stdio.h>

void UART_Send_Safe(char* str) {
    while(*str) {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, *str++);
    }
}

int main(void) {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
    NVIC_Configure();
    DMA_Configure();
    ADC_Configure();
    LCD_Init();
    USART2_Init();
    Alarm_Init();
    Alarm_Reset();

    AlarmState last_state = STATE_IDLE;

    while (1) {
        Alarm_Process();

        if (*p_alarm_state == STATE_ALARM_STOPPED && last_state != STATE_ALARM_STOPPED) {
            char report[60];
            sprintf(report, "\r\n[SUCCESS] Time: %d sec\r\n", (int)Alarm_GetElapsedSeconds());
            UART_Send_Safe(report);
            
            for(volatile uint32_t i=0; i<12000000; i++); // 약 3초 대기
            Alarm_Reset();
        }
        last_state = *p_alarm_state;
    }
}