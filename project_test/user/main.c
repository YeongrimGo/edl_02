#include "stm32f10x.h"
#include "lcd.h"
#include "alarm.h"          // alarm.h가 반드시 있어야 함
#include "inc/hw_config.h"  // hw_config.h가 반드시 있어야 함
#include <stdio.h>

// 안전한 UART 전송 함수
void UART_Send_Safe(char* str) {
    while(*str) {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, *str++);
    }
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

// 딜레이 함수
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

    UART_Send_Safe("\r\n[BOOT] System Ready! (Full Version)\r\n");

    AlarmState last_state = STATE_IDLE;

    while (1) {
        // 알람 로직 수행 (부저, LCD 갱신 등)
        Alarm_Process();

        // 상태가 '정지'로 바뀌었을 때 (인터럽트에 의해 변경됨)
        if (*p_alarm_state == STATE_ALARM_STOPPED && last_state != STATE_ALARM_STOPPED) {
            
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char report[60];
            sprintf(report, "\r\n[SUCCESS] Mission Clear! Time: %d sec\r\n", (int)final_time);
            UART_Send_Safe(report);

            // 성공 메시지를 3초간 유지
            Delay_ms(3000);
            
            // 시스템 리셋 및 대기
            Alarm_Reset();
            UART_Send_Safe("Reset Complete. Waiting for Bluetooth Command...\r\n");
        }

        last_state = *p_alarm_state;
    }
}