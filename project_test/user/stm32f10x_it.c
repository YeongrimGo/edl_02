#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h" // GPIO 사용을 위해 추가
#include "alarm.h"
#include "inc/hw_config.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint32_t* p_countdown_seconds;
extern volatile uint32_t* p_elapsed_seconds;
extern volatile AlarmState* p_alarm_state;

// hw_config.c에 정의된 모터 제어 변수 가져오기
extern volatile uint16_t motor_speed;
extern volatile int motor_state;

char rx_buffer[50];
uint8_t rx_index = 0;

// [추가] TIM3 인터럽트 핸들러: 여기서 980 PWM을 구현함
void TIM3_IRQHandler(void) {
    static uint16_t pwm_count = 0;

    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

        // 0~1000 카운트 반복
        pwm_count++;
        if (pwm_count >= 100) pwm_count = 0;

        // PWM 제어 로직 (Count가 Speed보다 작을 때만 ON)
        if (pwm_count < motor_speed) {
            switch (motor_state) {
                case 1: // Forward
                    // Left: IN1(PC10)=0, IN2(PC0)=1
                    GPIO_ResetBits(GPIOC, GPIO_Pin_10); GPIO_SetBits(GPIOC, GPIO_Pin_0);
                    // Right: IN3(PB7)=0, IN4(PB8)=1
                    GPIO_ResetBits(GPIOB, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_8);
                    break;
                case 2: // Backward
                    // Left: IN1(PC10)=1, IN2(PC0)=0
                    GPIO_SetBits(GPIOC, GPIO_Pin_10); GPIO_ResetBits(GPIOC, GPIO_Pin_0);
                    // Right: IN3(PB7)=1, IN4(PB8)=0
                    GPIO_SetBits(GPIOB, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_8);
                    break;
                case 3: // Turn Left
                    // Left Backward
                    GPIO_SetBits(GPIOC, GPIO_Pin_10); GPIO_ResetBits(GPIOC, GPIO_Pin_0);
                    // Right Stop
                    GPIO_ResetBits(GPIOB, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_8);
                    break;
                case 4: // Turn Right
                    // Left Stop
                    GPIO_ResetBits(GPIOC, GPIO_Pin_10); GPIO_ResetBits(GPIOC, GPIO_Pin_0);
                    // Right Backward
                    GPIO_SetBits(GPIOB, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_8);
                    break;
                case 0: // Stop
                default:
                    GPIO_ResetBits(GPIOC, GPIO_Pin_10 | GPIO_Pin_0);
                    GPIO_ResetBits(GPIOB, GPIO_Pin_7 | GPIO_Pin_8);
                    break;
            }
        } else {
            // PWM Off 기간: 모든 핀 끄기
            GPIO_ResetBits(GPIOC, GPIO_Pin_10 | GPIO_Pin_0);
            GPIO_ResetBits(GPIOB, GPIO_Pin_7 | GPIO_Pin_8);
        }
    }
}

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

void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (*p_alarm_state == STATE_ALARM_ACTIVE) {
            *p_alarm_state = STATE_WAIT_FOR_RAIN;
            Sensor_Mode_WaitRain();
            USART2_SendString("\r\nTouch Detected! Waiting for water...\r\n");
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

void ADC1_2_IRQHandler(void) {
    ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
}
