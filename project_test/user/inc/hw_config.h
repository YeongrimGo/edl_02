#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);
void DMA_Configure(void);
void ADC_Configure(void);

// [NEW] 센서 모드 제어 함수
void Sensor_Mode_WaitRain(void); // 터치 끄기, 빗물 감시 켜기
void Sensor_Mode_Reset(void);    // 초기화 (터치 켜기, 빗물 끄기)

#endif /* __HW_CONFIG_H */
