#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "alarm.h"
#include "inc/hw_config.h"
#include <stdio.h>

extern volatile AlarmState* p_alarm_state;

// 안전한 문자열 전송
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
    GPIO_Configure(); // 여기서 터치(PC1)와 빗물(PA1) 관련 설정 완료
    NVIC_Configure(); // 여기서 EXTI1, ADC 인터럽트 설정 완료
    ADC_Configure();  // 여기서 Watchdog 설정 완료
    DMA_Configure();
    LCD_Init();

    USART1_Init();
    USART2_Init();
    Alarm_Init();
    Alarm_Reset();

    UART_Send_Safe("\r\n[BOOT] System Ready! (Interrupt Mode)\r\n");

    // 이전 상태 추적용 변수
    AlarmState last_state = STATE_IDLE;

    while (1) {
        // 알람 소리 재생 및 LCD 표시는 여기서 계속 수행
        Alarm_Process();

        // 상태가 '정지'로 바뀌었을 때 (인터럽트에 의해 변경됨)
        if (*p_alarm_state == STATE_ALARM_STOPPED && last_state != STATE_ALARM_STOPPED) {

            uint32_t final_time = Alarm_GetElapsedSeconds();
            char report[60];
            sprintf(report, "\r\n[STOP] Mission Clear! Time: %d sec\r\n", (int)final_time);
            UART_Send_Safe(report);

            // 3초 대기 후 리셋
            Delay_ms(3000);
            Alarm_Reset();
            UART_Send_Safe("Reset Complete. Ready for next.\r\n");
        }

        last_state = *p_alarm_state;
    }
}
