#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h"
#include "alarm.h"
#include "inc/hw_config.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

extern volatile uint16_t motor_speed;
extern volatile int motor_state;

char rx_buffer[50];
uint8_t rx_index = 0;

void TIM3_IRQHandler(void) {
    static uint16_t pwm_count = 0;
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        pwm_count++;
        if (pwm_count >= 1000) pwm_count = 0;

        // 간단한 모터 PWM 제어 예시 (hw_config 참조 필요)
        // 여기서는 인터럽트 플래그 클리어만 확실히 처리
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

// [핵심] 블루투스 수신 핸들러
void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART2);
        USART_SendData(USART1, word); // 디버깅용

        if (rx_index < sizeof(rx_buffer) - 1) {
            // 엔터(\n, \r)를 만나면 명령 처리
            if (word == '\n' || word == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';

                    // 1. 숫자인지 확인 (알람 시간 설정)
                    int received_val = atoi(rx_buffer);

                    if (received_val > 0) {
                        // 숫자면 알람 카운트다운 시작
                        Alarm_Start((uint16_t)received_val);
                    }
                    else {
                        // 2. 숫자가 아니면 (예: "ok", "connect", "c") -> 연결 상태로 전환
                        // 현재 IDLE 상태일 때만 전환
                        if (Alarm_GetState() == STATE_IDLE) {
                            Alarm_Set_BT_Connected();
                        }
                    }
                    rx_index = 0;
                }
            }
            // 데이터 버퍼에 저장
            else {
                rx_buffer[rx_index++] = (char)word;
            }
        } else {
            rx_index = 0;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        // 빗물 감지 대기 상태로 전환 (알람 울릴 때 터치 시)
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_WAIT_FOR_RAIN;
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}
