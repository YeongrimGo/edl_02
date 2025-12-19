#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void RCC_Configure(void);
void GPIO_Configure(void);
void Ultrasonic_Configure(void); // [추가됨] 초음파 센서 설정
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);
void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);
void Sensor_Mode_WaitRain(void);
void Sensor_Mode_Reset(void);

// [추가됨] 초음파 센서 거리 측정 함수
uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id);

#endif
