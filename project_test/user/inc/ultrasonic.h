#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#include "stm32f10x.h"

void Ultrasonic_Configure(void);
uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id);

#endif

