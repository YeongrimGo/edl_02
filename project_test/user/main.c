#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "alarm.h"
#include "inc/hw_config.h" 
#include <stdio.h>

extern volatile AlarmState* p_alarm_state;
static uint8_t touch_pressed = 0;
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

            // 알람이 울리고 있을 때 (STATE_ALARM_ACTIVE)
            if (Alarm_GetState() == STATE_ALARM_ACTIVE) {

                // 1단계: 터치 센서(PC1) 확인
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                    if (touch_pressed == 0) {
                        touch_pressed = 1; // 터치됨을 기록
                        USART2_SendString("Touch detected! Now use water sensor.\r\n");
                    }
                }

                // 2단계: 터치가 이미 된 상태에서 빗물 센서(PA1) 확인
                // 빗물 센서는 물 감지 시 Low(0)를 출력하는 경우가 많으므로 Bit_RESET으로 체크 (센서 사양에 따라 SET으로 변경)
            if (touch_pressed == 1) {
                    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == Bit_RESET) {
                        // 물이 감지됨 (LOW)
                        *p_alarm_state = STATE_ALARM_STOPPED;
                        TIM_Cmd(TIM2, DISABLE);
                        touch_pressed = 0;
                        USART2_SendString("Step 2: Water Clear! Alarm Stopping...\r\n");
                    }
                }
            else {
                // 알람 상태가 아니면 플래그 항상 초기화
                touch_pressed = 0;
            }

            // 결과 보고 및 리셋 (기존 코드 유지)
            if (Alarm_GetState() == STATE_ALARM_STOPPED) {
                uint32_t final_time = Alarm_GetElapsedSeconds();
                char report[60];
                sprintf(report, "\r\n[STOP] Mission Clear! Time: %d sec\r\n", (int)final_time);
                USART2_SendString(report);

                for(volatile int i=0; i<5000000; i++);
                Alarm_Reset();
            }
        }
}
