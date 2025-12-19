#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "alarm.h"
#include "inc/hw_config.h" 
#include <stdio.h>

extern volatile AlarmState* p_alarm_state;

int main(void) {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
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

    USART2_SendString("System Ready! Enter seconds to set alarm.\r\n");

    while (1) {
        Alarm_Process();

        // 1. 알람이 울리는 중일 때 터치 센서 확인
        if (Alarm_GetState() == STATE_ALARM_ACTIVE) {
            // 터치 센서(PC1)가 감지되면 즉시 알람 정지 상태로 전환
            if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                // 알람 정지 및 상태 업데이트 (it.c의 버튼 로직과 동일 효과)
                *p_alarm_state = STATE_ALARM_STOPPED;
                TIM_Cmd(TIM2, DISABLE);
            }
        }

        // 2. 알람이 정지되었을 때 소요 시간 보고
        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            uint32_t final_time = Alarm_GetElapsedSeconds();
            char report[60];

            // 블루투스로 최종 소요 시간 전송
            sprintf(report, "\r\n[STOP] Alarm duration: %d seconds\r\n", (int)final_time);
            USART2_SendString(report);

            // 잠시 대기 후 시스템 초기화
            for(int i=0; i<3000000; i++);
            Alarm_Reset();
        }
    }
}
