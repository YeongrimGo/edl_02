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
    Alarm_Reset();

    // [요청] 블루투스 연결 완료 메시지 (영어)
    USART2_SendString("\r\nBluetooth Connected. Please send the alarm time in seconds.\r\n");

    // ============================================================
    // [DEBUG] 모터 동작 테스트 (전진 -> 후진 -> 좌 -> 우 각각 3초)
    // ============================================================
    LCD_Clear(BLACK);
    LCD_ShowString(40, 100, (u8*)"MOTOR TEST...", GREEN, BLACK);
    USART2_SendString("\r\n[DEBUG] Motor Test Start...\r\n");

    // 1. 전진 3초
    USART2_SendString("[DEBUG] Forward (3s)\r\n");
    LCD_ShowString(40, 130, (u8*)"Forward", WHITE, BLACK);
    Motor_Forward();
    for(volatile int i = 0; i < 36000000; i++); // 약 3초 지연

    // 2. 후진 3초
    USART2_SendString("[DEBUG] Backward (3s)\r\n");
    LCD_ShowString(40, 130, (u8*)"Backward", WHITE, BLACK);
    Motor_Backward();
    for(volatile int i = 0; i < 36000000; i++);

    // 3. 좌회전 3초
    USART2_SendString("[DEBUG] Turn Left (3s)\r\n");
    LCD_ShowString(40, 130, (u8*)"Turn Left", WHITE, BLACK);
    Motor_TurnLeft();
    for(volatile int i = 0; i < 36000000; i++);

    // 4. 우회전 3초
    USART2_SendString("[DEBUG] Turn Right (3s)\r\n");
    LCD_ShowString(40, 130, (u8*)"Turn Right", WHITE, BLACK);
    Motor_TurnRight();
    for(volatile int i = 0; i < 36000000; i++);

    // 5. 정지
    Motor_Stop();
    LCD_Clear(WHITE);
    USART2_SendString("[DEBUG] Motor Test Done.\r\n");
    // ============================================================

    // 기존 로직 복귀를 위해 알람 상태 리셋
    Alarm_Reset();

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
