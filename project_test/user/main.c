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

            // 터치 센서(PC1) 확인: 알람 중일 때만 작동
            if (Alarm_GetState() == STATE_ALARM_ACTIVE) {
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                    *p_alarm_state = STATE_ALARM_STOPPED;
                    TIM_Cmd(TIM2, DISABLE);
                }
            }

            // 결과 보고 및 리셋
            if (Alarm_GetState() == STATE_ALARM_STOPPED) {
                uint32_t final_time = Alarm_GetElapsedSeconds();
                char report[60];
                sprintf(report, "\r\n[STOP] Duration: %d sec\r\n", (int)final_time);
                USART2_SendString(report);

                for(volatile int i=0; i<5000000; i++); // 결과 확인용 지연
                Alarm_Reset();
            }
        }
    }
}
