#include "stm32f10x.h"
#include "alarm.h"
#include "inc/hw_config.h"
#include "lcd.h"
#include <stdio.h>

volatile uint32_t ADC_Value[2]; // 데이터 저장소

int main(void) {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
    ADC_Configure();
    DMA_Configure();
    LCD_Init();
    USART2_Init();
    Alarm_Init();
    Alarm_Reset();

    while (1) {
        Alarm_Process();

        // 알람이 종료 상태면 결과 보고 후 리셋
        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            char rpt[50];
            sprintf(rpt, "Alarm OFF! Time: %d sec\r\n", (int)Alarm_GetElapsedSeconds());
            USART2_SendString(rpt);

            for(volatile int i=0; i<5000000; i++);
            Alarm_Reset();
        }
    }
}
