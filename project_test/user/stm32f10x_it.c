#include "stm32f10x_it.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_tim.h"
#include "alarm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern volatile AlarmState* p_alarm_state;
extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;

// 블루투스 전송 함수 선언
extern void USART2_SendString(const char* str);

// 수신 버퍼
char rx_buffer[50];
uint8_t rx_index = 0;

// 터치 상태 플래그 (알람 중에만 유효)
volatile uint8_t touch_detected = 0;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) (*p_countdown_seconds)--;
            if (*p_countdown_seconds == 0) *p_alarm_state = STATE_ALARM_ACTIVE;
        } else if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            (*p_elapsed_seconds)++;
        }
    }
}

// 터치 센서 (PC1) 핸들러
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        // 알람이 울리는 중이고, 아직 터치가 처리되지 않았을 때만 동작
        if (*p_alarm_state == STATE_ALARM_ACTIVE && touch_detected == 0) {
            touch_detected = 1; // 중복 방지

            // [touched] 전송 (Flush는 SendString 함수 내 while 루프가 보장)
            USART2_SendString("[touched]\r\n");

            // 빗물 감지 센서(ADC) 인터럽트 활성화
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// 빗물 감지 (Analog Watchdog) 핸들러
void ADC1_2_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {
        // 터치가 선행되었고 알람이 울리는 중인지 확인
        if (touch_detected == 1 && *p_alarm_state == STATE_ALARM_ACTIVE) {

            // 알람 정지 및 시간 전송
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);

            char msg[50];
            sprintf(msg, "Alarm Stopped! Time: %d sec\r\n", (int)*p_elapsed_seconds);
            USART2_SendString(msg);

            // 상태 리셋
            touch_detected = 0;
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE); // ADC 인터럽트 끄기

            // 잠시 후 알람 초기화
            for(volatile int i=0; i<5000000; i++);
            Alarm_Reset();
        }
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);

        if (rx_index < sizeof(rx_buffer) - 1) {
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    int received_seconds = atoi(rx_buffer);

                    if (received_seconds > 0) {
                        char ack[40];
                        sprintf(ack, "Set Alarm: %d sec\r\n", received_seconds);
                        USART2_SendString(ack);
                        Alarm_Start((uint16_t)received_seconds);
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

void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
       USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
