#include "stm32f10x.h"
#include "lcd.h"
#include "alarm.h"          // 여기에 p_alarm_state가 선언되어 있음
#include "inc/hw_config.h"
#include <stdio.h>

// 안전한 UART 전송
void UART_Send_Safe(char* str) {
    while(*str) {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, *str++);
    }
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

void Delay_ms(uint32_t ms) {
    for(volatile uint32_t i = 0; i < ms * 4000; i++);
}

int main(void) {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
    NVIC_Configure();
    ADC_Configure(); 
    DMA_Configure();
    LCD_Init();

    USART1_Init();
    USART2_Init();
    Alarm_Init();
    Alarm_Reset();

    UART_Send_Safe("\r\n[BOOT] System Ready! (Final Version)\r\n");

    AlarmState last_state = STATE_IDLE;

    while (1) {
        // 알람 로직 수행 (부저, LCD)
        Alarm_Process();

        // 상태가 '정지'로 바뀌었을 때 (인터럽트에 의해 변경됨)
        // p_alarm_state 포인터가 가리키는 값을 읽어옴
        if (*p_alarm_state == STATE_ALARM_STOPPED && last_state != STATE_ALARM_STOPPED) {
            
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char report[60];
            sprintf(report, "\r\n[SUCCESS] Mission Clear! Time: %d sec\r\n", (int)final_time);
            UART_Send_Safe(report);

            // 3초 대기 후 리셋
            Delay_ms(3000);
            
            Alarm_Reset();
            UART_Send_Safe("Reset Complete. Waiting for Bluetooth Command...\r\n");
        }

        last_state = *p_alarm_state;
    }
}