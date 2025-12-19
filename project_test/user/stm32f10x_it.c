#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "alarm.h"
#include <string.h>
#include <stdlib.h>

// --- Alarm ����� ������ ---
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

// --- ���� ���� ---
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

// 1. Ÿ�̸� ���ͷ�Ʈ: ī��Ʈ�ٿ� �� ��� �ð� ����
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) {
                (*p_countdown_seconds)--;
            }
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE; // ���� �︲ ����
            }
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++; // ���� �︰ �ð� ����
        }
    }
}

// 2. ��ư ���ͷ�Ʈ (PA0): �˶� ����
void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        // �˶��� �︮�� ���� ���� ��ư ����
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED; // ���� �������� ���� �� ����
            TIM_Cmd(TIM2, DISABLE); // Ÿ�̸� ����
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// 3. USART1 (PC -> STM32): PuTTY���� Ÿ������ ������ ��������� �н�����
void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        
        // PC ȭ�鿡 ���� (Ÿ�����Ѱ� ���̰�)
        USART_SendData(USART1, word);
        
        // ������� ���� ���� (AT Ŀ�ǵ� ���� ��)
        USART_SendData(USART2, word);
        
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

// 4. USART2 (������� -> STM32): ��� �Ľ� �� PC�� ����͸�
void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        
        // 1. ������� ���� ���� �����͸� PC(PuTTY)�� ���
        USART_SendData(USART1, word); 

        // 2. ���ۿ� ���� �� ��ɾ� ó��
        if (word == '\n' || word == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';

                // atoi로 변환된 숫자를 '초' 단위로 직접 사용
                int received_val = atoi(rx_buffer);

                if (received_val > 0) {
                    // 이제 입력받은 숫자가 10이면 10초, 60이면 60초로 설정됩니다.
                    Alarm_Start((uint16_t)received_val);

                    // 확인용 로그 (선택 사항)
                    char log[30];
                    sprintf(log, "\r\nTimer Set: %d Seconds\r\n", received_val);
                    USART2_SendString(log);
                }

                rx_index = 0;
            }
        } else {
                // ���� ���ڸ� ���ۿ� ��� (Ȥ�� ���� ���� �� �ʿ�� ���� �߰�)
                if (word >= '0' && word <= '9') {
                    rx_buffer[rx_index++] = (char)word;
                }
            }
        } else {
            rx_index = 0; // ���� �����÷ο� ����
        }
        
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
