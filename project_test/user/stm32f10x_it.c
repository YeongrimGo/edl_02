#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "inc/alarm.h"
#include "inc/hw_config.h"
#include "inc/usart_comm.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

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
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
            (*p_elapsed_seconds)++;
        }
    }
}

void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
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

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        USART_SendData(USART1, word);

        Alarm_HandleInput((char)word);

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_WAIT_FOR_RAIN;
            Sensor_Mode_WaitRain();
            USART2_SendString("\r\nTouch Detected! Waiting for rain...\r\n");
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

void ADC1_2_IRQHandler(void) {
    ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
}
