#include "stm32f10x_it.h"
#include "alarm.h"
#include <stdlib.h>

static char rx_buf[16];
static uint8_t rx_idx = 0;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) (*p_countdown_seconds)--;
            else *p_alarm_state = STATE_ALARM_ACTIVE;
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++;
        }
    }
}

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t data = USART_ReceiveData(USART2);
        if (data >= '0' && data <= '9') {
            rx_buf[rx_idx++] = (char)data;
        } else if (data == '\r' || data == '\n') {
            rx_buf[rx_idx] = '\0';
            if (rx_idx > 0) Alarm_Start(atoi(rx_buf)); // 초 단위 설정
            rx_idx = 0;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
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
