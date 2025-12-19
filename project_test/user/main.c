#include "stm32f10x.h"
#include "lcd.h"
#include "alarm.h"          
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

    UART_Send_Safe("\r\n[BOOT] System Ready! (Safe Logic)\r\n");

    AlarmState last_state = STATE_IDLE;
    char debug_buf[30];

    while (1) {
        // 알람 로직 수행
        Alarm_Process();

        // [디버깅] 현재 빗물 센서 값 표시 (이 값이 1000 이하여야 비로 인식)
        // 만약 마른 상태에서 500~800이라면 센서 문제거나 임계값을 더 낮춰야 함
        sprintf(debug_buf, "Rain Sensor: %d", (int)ADC_Value[0]);
        LCD_ShowString(10, 10, (u8*)debug_buf, BLACK, WHITE);

        // 상태가 '정지'로 바뀌었을 때
        if (*p_alarm_state == STATE_ALARM_STOPPED && last_state != STATE_ALARM_STOPPED) {
            
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char report[60];
            sprintf(report, "\r\n[SUCCESS] Mission Clear! Time: %d sec\r\n", (int)final_time);
            UART_Send_Safe(report);

            Delay_ms(3000);
            
            Alarm_Reset();
            UART_Send_Safe("Reset Complete. Waiting for Bluetooth...\r\n");
        }

        last_state = *p_alarm_state;
    }
}