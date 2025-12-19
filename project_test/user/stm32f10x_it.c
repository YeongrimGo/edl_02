#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "alarm.h"
#include "inc/hw_config.h"
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

static char rx_buffer[50];
static uint8_t rx_index = 0;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) (*p_countdown_seconds)--;
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE;
                BUZZER_On(); 
            }
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++;
        }
    }
}

void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            BUZZER_Off();
            TIM_Cmd(TIM2, DISABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET); // 전송 대기
        USART_SendData(USART2, word);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

void USART2_IRQHandler(void) {
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) { // Overrun 에러 방지
        USART_ReceiveData(USART2);
    }
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // 전송 대기
        USART_SendData(USART1, word);
        if (rx_index < 49) {
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    int val = atoi(rx_buffer);
                    if (val > 0) Alarm_Start((uint16_t)val);
                    rx_index = 0;
                }
            } else if (word >= '0' && word <= '9') {
                rx_buffer[rx_index++] = (char)word;
            }
        } else rx_index = 0;
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}