#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

// main.c나 다른 곳에서 참조할 전역 변수
extern volatile uint32_t ADC_Value[1];

// 함수 선언 (목차)
void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);

// [중요] 이 선언들이 있어야 main.c에서 에러가 안 납니다.
void USART1_Init(void);
void USART2_Init(void);

#endif