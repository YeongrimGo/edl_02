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

char rx_buffer[50];
uint8_t rx_index = 0;

// 1. 타이머 인터럽트
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) {
                (*p_countdown_seconds)--;
            }
            
            // [중요] 카운트가 0이 되어 알람이 시작되는 순간!
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE;

                // [핵심 로직 추가] 알람 시작 시점에 이미 손으로 잡고(Touch) 있는지 확인
                // EXTI(Edge Trigger)는 변화가 없으면 발생 안하므로 여기서 직접 상태(Level) 검사
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                    // 이미 잡고 있다면 바로 빗물 센서 켬
                    ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
                    ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
                }
            }
        } 
        else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++;
        }
        
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

// 2. 터치 센서 인터럽트 (알람 도중 새로 터치할 때)
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 빗물 감지 Watchdog 활성화
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// 3. 빗물 감지 인터럽트 (Analog Watchdog)
void ADC1_2_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {
        // 알람이 울리는 중이라면 -> 정지!
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE); // 임무 완료 후 비활성화
        }
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}

// 4. Bluetooth 수신
void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        char word = USART_ReceiveData(USART2);
        
        if (word == '\n' || word == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                int received_val = atoi(rx_buffer);
                if (received_val > 0) {
                    Alarm_Start((uint16_t)received_val);
                    // 새 시작 시 빗물 감지 초기화
                    ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
                }
                rx_index = 0;
            }
        } else if (word >= '0' && word <= '9') {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = word;
            }
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}