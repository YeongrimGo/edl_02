#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

// 기존에 있던 선언들...
void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);
void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);

// [여기 추가] 아래 두 줄이 없어서 에러가 난 것입니다.
void Sensor_Mode_WaitRain(void);
void Sensor_Mode_Reset(void);

#endif
