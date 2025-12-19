#ifndef __HW_CONFIG_H__
#define __HW_CONFIG_H__

#include "stm32f10x.h"
#include <stdint.h>

void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void DMA_Configure(void);
void ADC_Configure(void);

void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);

/* Buzzer on PC6 (TIM3_CH1 Full Remap), Low-Trigger */
void BUZZER_Configure(void);
void BUZZER_Start(uint16_t freq_hz);
void BUZZER_Stop(void);

#endif
