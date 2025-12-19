#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

// main.c에서 디버깅용으로 빗물 센서 값을 볼 수 있게 공유
extern volatile uint32_t ADC_Value[1];

// 함수 선언부 (Prototype)
void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);

// UART 초기화 함수 선언
void USART1_Init(void);
void USART2_Init(void);

#endif