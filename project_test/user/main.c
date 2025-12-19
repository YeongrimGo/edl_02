#include "stm32f10x.h"
#include "inc/hw_config.h" // 하드웨어 설정 헤더 포함
#include "lcd.h"
#include "alarm.h"
#include "stm32f10x_usart.h"
#include <stdio.h>

// 전역 변수 (ADC 값 저장용 - 참조용)
volatile uint32_t ADC_Value[1];

// 외부 참조
extern void USART2_SendString(const char* str);

int main(void) {
    SystemInit();

    // hw_config.c에 정의된 설정 함수들 호출
    RCC_Configure();
    GPIO_Configure();
    EXTI_Configure(); // 터치 센서용 인터럽트 설정
    ADC_Configure();  // 빗물 센서용 ADC(Watchdog) 설정
    NVIC_Configure();

    LCD_Init();

    // 통신 초기화
    USART1_Init(); // PC 디버그용
    USART2_Init(); // 블루투스용

    // 알람 초기화
    Alarm_Init();
    Alarm_Reset();

    // 6. 재시작 시 [boot] 전송
    USART2_SendString("[boot]\r\n");

    while (1) {
        // 메인 루프는 오직 알람 재생 및 LCD 표시만 담당
        // 모든 센서 처리는 인터럽트(stm32f10x_it.c)에서 수행됨
        Alarm_Process();
    }
}
