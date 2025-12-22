#ifndef __USART_COMM_H
#define __USART_COMM_H

#include "stm32f10x.h"

void USART1_Init(void);
void USART2_Init(void);
void USART2_SendString(const char* str);

#endif

