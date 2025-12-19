#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "alarm.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

// [누락되었던 변수 선언 추가]
char rx_buffer[50];
uint8_t rx_index = 0;

// 1. 타이머 인터럽트 (기존 유지)
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

// 2. EXTI0 (알람 정지 버튼 PA0 - 기존 유지)
void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 버튼을 누르면 상태만 변경하여 메인 루프에서 보고하게 함
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);
            // 알람이 꺼졌으므로 빗물 감지 인터럽트도 비활성화
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// [신규] 3. EXTI1 (터치 센서 인터럽트 PC1)
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {

        // 알람이 울리는 중에만 작동
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 터치가 감지됨 -> 빗물 센서 인터럽트(Watchdog) 활성화!
            // 이전에 남아있을 수 있는 플래그 클리어
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
            // 빗물 감지 인터럽트 켜기
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
        }

        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// [신규] 4. ADC (빗물 센서 Watchdog 인터럽트 PA1)
void ADC1_2_IRQHandler(void) {
    // Analog Watchdog 플래그 확인 (빗물 감지됨)
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {

        // 알람이 울리는 중이라면 알람 정지
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);

            // 미션 성공 후에는 빗물 감지 인터럽트 다시 끄기 (반복 실행 방지)
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
        }

        // 플래그 클리어
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
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
                        // 새 알람 시작 시 빗물 인터럽트 비활성화 (초기화)
                        ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
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
