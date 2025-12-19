#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void RCC_Configure(void);
void GPIO_Configure(void);
void Ultrasonic_Configure(void); // 초음파 설정
void Motor_Configure(void);      // [추가] 모터 핀 설정
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);
void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);
void Sensor_Mode_WaitRain(void);
void Sensor_Mode_Reset(void);

uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id);

// [추가] 모터 동작 함수들
void Motor_Forward(void);
void Motor_Backward(void);
void Motor_TurnLeft(void);
void Motor_TurnRight(void);
void Motor_Stop(void);

#endif
