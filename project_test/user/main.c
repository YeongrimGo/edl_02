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

#define RAIN_THRESHOLD 2000

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

                // 알람이 울리고 있을 때
                if (Alarm_GetState() == STATE_ALARM_ACTIVE) {

                    // 1단계: 터치 센서 확인
                if (touch_pressed == 0) {
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {

                    touch_pressed = 1; // 즉시 인정
                    UART_Send_Safe("[DEBUG] Touch Detected!\r\n");

                    // 한 번 눌린 후에는 채터링(떨림) 방지를 위해 0.3초간 입력 무시
                    Delay_ms(300);
                    }
                }

                    // 2단계: 터치 후 빗물 센서 확인 (아날로그 방식)
                    if (touch_pressed == 1) {
                        // [변경됨] 디지털 핀 읽기 -> ADC 값 비교
                        // 빗물이 묻어서 ADC 값이 임계값보다 떨어지면(또는 오르면) 감지
                        // 만약 물 묻었을 때 감지가 안 되면 부등호를 '>'로 바꿔보세요.
                        if (ADC_Value[0] < RAIN_THRESHOLD) {

                            *p_alarm_state = STATE_ALARM_STOPPED;
                            TIM_Cmd(TIM2, DISABLE);
                            // 디버깅용으로 현재 ADC 값 출력해보기
                            char debug_msg[50];
                            sprintf(debug_msg, "Water Detected! ADC Val: %d\r\n", (int)ADC_Value[0]);
                            USART2_SendString(debug_msg);
                        }
                    }
                } else {
                    touch_pressed = 0;
                }

                // 결과 보고 및 리셋
                if (Alarm_GetState() == STATE_ALARM_STOPPED) {
                    touch_pressed = 0;
                    uint32_t final_time = Alarm_GetElapsedSeconds();
                    char report[60];
                    sprintf(report, "\r\n[STOP] Mission Clear! Time: %d sec\r\n", (int)final_time);
                    UART_Send_Safe(report);

                    for(volatile int i=0; i<5000000; i++);
                    Alarm_Reset();
                }
            }
}
