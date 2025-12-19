#include "stm32f10x_it.h"
#include "alarm.h"
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;
char rx_buffer[50];
uint8_t rx_index = 0;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) (*p_countdown_seconds)--;
            else *p_alarm_state = STATE_ALARM_ACTIVE;
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++;
        }
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

// 터치 센서 인터럽트
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE); // 이제부터 빗물 감지 시작
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// 빗물 센서 Analog Watchdog 인터럽트
void ADC1_2_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE); // 임무 완수 후 비활성화
        }
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        char word = USART_ReceiveData(USART2);
        if (word == '\n' || word == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                Alarm_Start(atoi(rx_buffer));
                rx_index = 0;
            }
        } else if (word >= '0' && word <= '9') {
            rx_buffer[rx_index++] = word;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}