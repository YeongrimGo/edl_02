// main.c
#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "alarm.h"
#include "inc/hw_config.h" 
#include <stdio.h>

// --- Global variables ---
volatile uint32_t ADC_Value[1];

// 프로토타입 선언 (hw_config.c에 정의됨)
void USART1_Init(void); 

int main(void) {
    SystemInit();
    
    // 1. 하드웨어 설정
    RCC_Configure();
    GPIO_Configure();
    NVIC_Configure();
    
    // 2. 주변기기 초기화
    ADC_Configure();
    DMA_Configure();
    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();
    
    USART1_Init(); // PC(PuTTY) 연결용
    USART2_Init(); // 블루투스 연결용
    
    Alarm_Init();
    Alarm_Reset();

    // 부팅 메시지를 PC와 블루투스로 전송 (확인용)
    USART2_SendString("System Ready!\r\n");

    while (1) {
        // 알람 상태 머신 실행 (LCD 업데이트, 부저 제어 등)
        Alarm_Process();

        // 3. 알람이 사용자에 의해 정지되었는지 확인
        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            uint32_t final_elapsed_seconds = Alarm_GetElapsedSeconds();
            char msg[50];
            
            // 경과 시간을 문자열로 변환 (예: "Time: 5 secs\r\n")
            sprintf(msg, "Time: %d secs\r\n", final_elapsed_seconds);
            
            // *** 중요: 블루투스 모듈(USART2)로 전송 ***
            USART2_SendString(msg);

            // 상태를 IDLE로 리셋하여 다음 명령 대기
            Alarm_Reset();
        }
    }
}