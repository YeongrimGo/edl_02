#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h"
#include "alarm.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

// [확인용] 빗물 센서 값이 이 값보다 낮아야 진짜 물로 인정함 (오작동 방지)
#define RAIN_CHECK_LEVEL 1200 

char rx_buffer[50];
uint8_t rx_index = 0;

// 1. 타이머 인터럽트
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        
        if (*p_alarm_state == STATE_COUNTDOWN) {
            if (*p_countdown_seconds > 0) {
                (*p_countdown_seconds)--;
            }
            
            // 알람 시작되는 순간!
            if (*p_countdown_seconds == 0) {
                *p_alarm_state = STATE_ALARM_ACTIVE;

                // [Holding 감지] 이미 손잡이를 잡고 있는지 확인
                if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_SET) {
                    // 중요: 빗물 감지 켜기 전에 기존 플래그 싹 지움
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

// 2. 터치 센서 인터럽트
void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            // 터치 감지됨 -> 빗물 센서 활성화 준비
            
            // [중요] 켜는 순간 바로 꺼지는 걸 막기 위해 플래그 먼저 지움
            ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
            
            // 이제 빗물 감지 시작
            ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

// 3. 빗물 감지 인터럽트 (Analog Watchdog)
void ADC1_2_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET) {
        
        // [이중 확인] 진짜 물이 묻었는지 ADC 값을 직접 읽어서 확인
        // 노이즈나 튀는 값 때문에 꺼지는걸 방지
        uint16_t real_check = ADC_GetConversionValue(ADC1);

        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            
            // 진짜 젖어서 값이 떨어진게 맞으면 알람 정지
            if (real_check < RAIN_CHECK_LEVEL) {
                *p_alarm_state = STATE_ALARM_STOPPED;
                TIM_Cmd(TIM2, DISABLE);
                
                // 임무 완료 후 빗물 감지 끔
                ADC_ITConfig(ADC1, ADC_IT_AWD, DISABLE);
            }
        }
        // 플래그 클리어
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}

// 4. 블루투스 수신
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
            if (rx_index < 49) rx_buffer[rx_index++] = word;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}