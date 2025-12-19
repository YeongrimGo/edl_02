#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "alarm.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

char rx_buffer[50];
uint8_t rx_index = 0;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) {
                (*p_countdown_seconds)--;
            }
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE;
            }
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++; // 알람이 울리는 동안 초당 1씩 증가
        }
    }
}

void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 버튼을 누르면 상태만 변경하여 메인 루프에서 보고하게 함
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        USART_SendData(USART1, word);
        USART_SendData(USART2, word);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

// ... 기존 헤더 생략 ...

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        USART_SendData(USART1, word); // PC 모니터링용

        if (rx_index < sizeof(rx_buffer) - 1) {
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    int received_val = atoi(rx_buffer);

                    if (received_val > 0) {
                        // 여기서 입력받은 숫자(received_val)가 바로 초(sec)가 됩니다.
                        Alarm_Start((uint16_t)received_val);
                    }
                    rx_index = 0;
                }
            } else if (word >= '0' && word <= '9') {
                rx_buffer[rx_index++] = (char)word;
            }
        } else {
            rx_index = 0;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

// stm32f10x_it.c 맨 아래 혹은 적절한 위치에 추가

// [NEW] 터치 센서용 인터럽트 핸들러 (PC1 -> EXTI1)
void EXTI1_IRQHandler(void) {
    // EXTI Line 1에서 인터럽트가 발생했는지 확인
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {

        // 알람이 울리는 중(ACTIVE)일 때만 동작
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE); // 타이머 정지
        }

        // 인터럽트 플래그 클리어 (필수)
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}
