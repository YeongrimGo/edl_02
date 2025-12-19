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

// 프로토타입 선언
void USART1_Init(void);

int main(void) {
    // 1. 시스템 초기화 및 하드웨어 설정
    SystemInit();
    RCC_Configure();
    GPIO_Configure(); // 여기서 PB0(부저), PC1(터치센서)이 설정되어 있어야 합니다.
    NVIC_Configure();

    // 2. 주변기기 초기화
    ADC_Configure();
    DMA_Configure();
    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();

    USART1_Init(); // PC 디버깅용
    USART2_Init(); // 블루투스용

    Alarm_Init();
    Alarm_Reset();

    // 터치 감지용 변수
    uint32_t touch_start_time = 0;
    uint8_t is_touching = 0;

    USART2_SendString("System Ready! Touch PC1 for 3s to stop alarm.\r\n");

    while (1) {
            Alarm_Process();

            // --- 터치센서 로직 (3초 누름 해제 및 즉시 종료로 변경 가능) ---
            if (Alarm_GetState() == STATE_ALARM_ACTIVE) {
                // 터치센서(PC1)가 눌리면 즉시 알람 종료 (요청하신 대로 단순화)
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                    // 알람 상태를 STOPPED로 변경하여 아래 로그 출력 로직이 실행되게 함
                    // (내부적으로 Alarm_Reset() 대신 상태만 변경하거나 필요에 따라 수정)
                    *p_alarm_state = STATE_ALARM_STOPPED;
                    TIM_Cmd(TIM2, DISABLE); // 타이머 정지
                }
            }

            // --- 알람 종료 시 소요 시간 전송 로직 ---
            if (Alarm_GetState() == STATE_ALARM_STOPPED) {
                // Alarm_GetElapsedSeconds()는 알람이 울린 순간부터 카운트된 초를 반환합니다.
                uint32_t final_elapsed_seconds = Alarm_GetElapsedSeconds();
                char msg[60];

                // 블루투스로 꺼지기까지 걸린 시간 전송
                sprintf(msg, "\r\n[ALARM OFF] Elapsed Time: %d sec\r\n", (int)final_elapsed_seconds);
                USART2_SendString(msg);

                // 처리 후 시스템 초기화 (IDLE 상태로 복귀)
                for(int i=0; i<1000000; i++); // 짧은 대기
                Alarm_Reset();
            }
        }
    }
}
