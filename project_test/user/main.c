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
        // 알람 상태 머신 실행 (LCD 업데이트 및 수동 부저 신호 발생)
        Alarm_Process();

        // --- 터치센서 3초 누름 로직 ---
        // 알람이 실제 울리고 있는 상태에서만 체크
        if (Alarm_GetState() == STATE_ALARM_ACTIVE) {

            // 터치센서(PC1)가 눌렸는지 확인 (High Active 기준)
            if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                if (!is_touching) {
                    // 터치가 시작된 순간의 경과 시간을 기록
                    touch_start_time = Alarm_GetElapsedSeconds();
                    is_touching = 1;
                }

                // 현재 경과 시간과 시작 시간의 차이가 3초 이상인지 확인
                if ((Alarm_GetElapsedSeconds() - touch_start_time) >= 3) {
                    Alarm_Reset(); // 알람 초기화 및 부저 정지
                    is_touching = 0;
                    USART2_SendString("Alarm Stopped: Touch Sensor 3s Pressed\r\n");
                }
            } else {
                // 손을 떼면 카운트 초기화
                is_touching = 0;
            }
        } else {
            // 알람 상태가 아니면 변수 초기화
            is_touching = 0;
        }

        // --- 기존 알람 정지 후 로그 출력 로직 ---
        if (Alarm_GetState() == STATE_ALARM_STOPPED) {
            uint32_t final_elapsed_seconds = Alarm_GetElapsedSeconds();
            char msg[50];
            sprintf(msg, "Alarm finished. Total time: %d sec\r\n", (int)final_elapsed_seconds);
            USART2_SendString(msg);

            // 일정 시간 대기 후 다시 IDLE 상태로 전환 (사용자 확인용)
            for(int i=0; i<2000000; i++);
            Alarm_Reset();
        }
    }
}
