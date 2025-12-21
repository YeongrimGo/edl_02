#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "alarm.h"
#include "inc/hw_config.h" 
#include <stdio.h>

extern volatile AlarmState* p_alarm_state;

volatile uint32_t ADC_Value[1];

int main(void) {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
    Ultrasonic_Configure();
    Motor_Configure();
    NVIC_Configure();
    ADC_Configure();
    DMA_Configure();
    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();
    USART1_Init();
    USART2_Init();
    Alarm_Init();

    // [수정] 초기 상태를 '블루투스 연결 대기'로 설정
    Alarm_Reset();
    *p_alarm_state = STATE_WAIT_BLUETOOTH; // 강제 설정

    // 주의: 초기 메시지는 PA0 버튼을 눌러 STATE_IDLE로 진입할 때 전송됨.

    while (1) {
        Alarm_Process();

        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char time_str[20];
            char report[80];

            // [수정] 00:00 포맷 적용
            Time_Format(final_time, time_str);

            sprintf(report, "\r\n[STOP] Duration: %s\r\n", time_str);
            USART2_SendString(report);

            for(volatile int i=0; i<5000000; i++);
            Alarm_Reset(); // 리셋 후에는 STATE_IDLE 상태가 되어 바로 다음 알람 입력 대기
        }
    }
}
