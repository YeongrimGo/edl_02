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

// USART 통신용 (hw_config.c 또는 main.c에 있는 것을 참조하거나 여기서 선언)
void USART2_SendString(const char* str);

// 1. 타이머 인터럽트 (기존 유지)
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

// 2. EXTI0 (강제 종료 버튼 - 기존 유지)
void EXTI0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);
            // 알람이 꺼졌으므로 빗물 감지 인터럽트도 비활성화
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// [신규] 3. EXTI1 (터치 센서 인터럽트)
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {

        // 알람이 울리는 중에만 작동
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 터치가 감지됨 -> 빗물 센서 인터럽트(Watchdog) 활성화!
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD); // 이전 플래그 클리어
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);  // 빗물 감시 시작

            // 디버깅 메시지 전송 (옵션)
            // USART2_SendString("[IT] Touch Detected! Rain Sensor Activated.\r\n");
        }

        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// [신규] 4. ADC (빗물 센서 Watchdog 인터럽트)
void ADC1_2_IRQHandler(void) {
    // Analog Watchdog 플래그 확인
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {

        // 알람이 울리는 중이라면 알람 정지
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_ALARM_STOPPED;
            TIM_Cmd(TIM2, DISABLE);

            // 미션 성공 후에는 빗물 감지 인터럽트 다시 끄기 (반복 실행 방지)
            ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);

            // 디버깅 메시지 (옵션)
            // USART2_SendString("[IT] Rain Detected! Alarm Stopped.\r\n");
        }

        // 플래그 클리어
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}

// ... USART1, USART2 핸들러는 기존 코드 그대로 유지 ...
void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint16_t word = USART_ReceiveData(USART1);
        USART_SendData(USART1, word);
        USART_SendData(USART2, word);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

void USART2_IRQHandler(void) {
    // 기존 코드 내용 그대로...
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
                        // 알람 시작 시 혹시 켜져 있을 빗물 인터럽트 끄기
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
