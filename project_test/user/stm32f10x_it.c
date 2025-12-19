#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "alarm.h"
#include "inc/hw_config.h" // Sensor_Mode 함수 사용
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
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
            // 알람 중이거나 빗물 대기 중일 때도 시간 카운트 (원하는 대로 조정 가능)
            (*p_elapsed_seconds)++;
        }
    }
}

void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE || *p_alarm_state == STATE_WAIT_FOR_RAIN) {
            // 물리 버튼은 비상 정지용으로 모든 상태에서 정지 가능하게 함
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

        if (rx_index < sizeof(rx_buffer) - 1) {
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    int received_val = atoi(rx_buffer);

                    if (received_val > 0) {
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

// 터치 센서용 인터럽트 핸들러 (PC1 -> EXTI1)
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {

        // 알람이 울리는 중(ACTIVE)일 때 터치되면
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // [LOGIC CHANGE] 알람을 끄지 않고, '빗물 대기 모드'로 전환
            *p_alarm_state = STATE_WAIT_FOR_RAIN;

            // 터치 센서 비활성화 & 빗물 센서(ADC) 인터럽트 활성화
            Sensor_Mode_WaitRain();

            USART2_SendString("\r\nTouch Detected! Waiting for rain...\r\n");
        }

        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// [NEW] 빗물 감지 센서용 인터럽트 (ADC Analog Watchdog)
void ADC1_2_IRQHandler(void) {
    // Analog Watchdog 이벤트 확인
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD)) {

        // 빗물을 기다리는 상태였다면
        if (*p_alarm_state == STATE_WAIT_FOR_RAIN) {
            // 알람 완전 정지
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);

            // 빗물 센서 인터럽트 끄기 (재동작 방지)
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);

            USART2_SendString("\r\nRain Detected! Alarm Stopped.\r\n");
        }

        // 플래그 클리어
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}
