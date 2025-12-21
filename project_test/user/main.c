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
    NVIC_Configure();
    ADC_Configure();
    DMA_Configure();
//    LCD_Init();
    Touch_Configuration();
    Motor_Configure();
//    Touch_Adjust();
    USART1_Init();
    USART2_Init();
    Alarm_Init();
    Alarm_Reset();

    // [요청] 블루투스 연결 완료 메시지 (영어)
    USART2_SendString("\r\nBluetooth Connected. Please send the alarm time in seconds.\r\n");

    while (1) {
        Alarm_Process();

        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char time_str[20];
            char report[80];

            // [요청] 00시 00분 00초 형식으로 변환
            Time_Format(final_time, time_str);

            sprintf(report, "\r\n[STOP] Duration: %s\r\n", time_str);
            USART2_SendString(report);

            for(volatile int i=0; i<5000000; i++);
            Alarm_Reset();
        }
    }
}
