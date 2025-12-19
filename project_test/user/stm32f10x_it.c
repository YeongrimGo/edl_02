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

void NMI_Handler(void) {}
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void) { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) {}

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) (*p_countdown_seconds)--;
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE;
                BUZZER_On(); // 카운트다운 완료 시 부저 On
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
            BUZZER_Off(); // 버튼 누르면 부저 Off
            TIM_Cmd(TIM2, DISABLE);
        }
        EXTI0_ClearITPendingBit(EXTI_Line0);
    }
}

void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        USART_SendData(USART1, word);
        USART_SendData(USART2, word); // PC에서 보낸 걸 블루투스로 전달
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

void USART2_IRQHandler(void) {
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        USART_SendData(USART1, word); // 블루투스에서 받은 걸 PC로 전달

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