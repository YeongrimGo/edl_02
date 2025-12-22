#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void RCC_Configure(void);
void GPIO_Configure(void);
void NVIC_Configure(void);
void ADC_Configure(void);
void DMA_Configure(void);
void Sensor_Mode_WaitRain(void);
void Sensor_Mode_Reset(void);

#endif
