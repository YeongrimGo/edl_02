#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "alarm.h"
#include <string.h>
#include <stdlib.h>

// --- Alarm 모듈의 변수들 ---
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

// --- 수신 버퍼 ---
char rx_buffer[50];
uint8_t rx_index = 0;

/******************************************************************************/
/* Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/
void NMI_Handler(void) {}
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void) { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) {}

/******************************************************************************/
/* STM32F10x Peripherals Interrupt Handlers                   */
/******************************************************************************/

// 1. 타이머 인터럽트: 카운트다운 및 경과 시간 측정
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) {
                (*p_countdown_seconds)--;
            }
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE; // 부저 울림 시작
            }
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++; // 부저 울린 시간 측정
        }
    }
}

// 2. 버튼 인터럽트 (PA0): 알람 끄기
void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        // 알람이 울리고 있을 때만 버튼 동작
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED; // 메인 루프에서 감지 후 전송
            TIM_Cmd(TIM2, DISABLE); // 타이머 정지
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// 3. USART1 (PC -> STM32): PuTTY에서 타이핑한 내용을 블루투스로 패스스루
void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        
        // PC 화면에 에코 (타이핑한거 보이게)
        USART_SendData(USART1, word);
        
        // 블루투스 모듈로 전송 (AT 커맨드 설정 등)
        USART_SendData(USART2, word);
        
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

// 4. USART2 (블루투스 -> STM32): 명령 파싱 및 PC로 모니터링
void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        
        // 1. 디버깅을 위해 받은 데이터를 PC(PuTTY)로 출력
        USART_SendData(USART1, word); 

        // 2. 버퍼에 저장 및 명령어 처리
        if (rx_index < sizeof(rx_buffer) - 1) {
            // 줄바꿈 문자나 캐리지 리턴을 만나면 명령어로 인식
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0'; // 문자열 종료

                    // 숫자인지 확인하고 타이머 설정 (예: "10" 수신 시)
                    int received_val = atoi(rx_buffer);
                    
                    if (received_val > 0) {
                        // 알람 시작 함수 호출 (alarm.c에 있다고 가정)
                        Alarm_Start((uint16_t)received_val);
                        
                        // PC에도 설정되었다고 로그 출력 (선택사항)
                        // char log[30];
                        // sprintf(log, "\r\nTimer Set: %d\r\n", received_val);
                        // for(int i=0; log[i]; i++) { USART_SendData(USART1, log[i]); while(USART_GetFlagStatus(USART1, USART_FLAG_TXE)==RESET); }
                    }
                    
                    rx_index = 0; // 버퍼 초기화
                }
            } else {
                // 숫자 문자만 버퍼에 담기 (혹은 공백 제거 등 필요시 로직 추가)
                if (word >= '0' && word <= '9') {
                    rx_buffer[rx_index++] = (char)word;
                }
            }
        } else {
            rx_index = 0; // 버퍼 오버플로우 방지
        }
        
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}