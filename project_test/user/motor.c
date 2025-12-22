#include "inc/motor.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

void Motor_Configure(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    // Left: PC0, PC10
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_10;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // Right: PB7, PB8
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 초기 정지
    Motor_Stop();
}


// Left: IN1(PC10), IN2(PC0) / Right: IN3(PB7), IN4(PB8)
// 앞뒤, 좌우 반전 적용된 상태(기존 동작 의미 유지)

void Motor_Forward(void) {
    // Left: IN1=0, IN2=1
    GPIO_ResetBits(GPIOC, GPIO_Pin_10);
    GPIO_SetBits  (GPIOC, GPIO_Pin_0);

    // Right: IN3=0, IN4=1
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_SetBits  (GPIOB, GPIO_Pin_8);
}

void Motor_Backward(void) {
    // Left: IN1=1, IN2=0
    GPIO_SetBits  (GPIOC, GPIO_Pin_10);
    GPIO_ResetBits(GPIOC, GPIO_Pin_0);

    // Right: IN3=1, IN4=0
    GPIO_SetBits  (GPIOB, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Motor_TurnLeft(void) {
    // Left: backward (IN1=1, IN2=0)
    GPIO_SetBits  (GPIOC, GPIO_Pin_10);
    GPIO_ResetBits(GPIOC, GPIO_Pin_0);

    // Right: stop (IN3=0, IN4=0)
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Motor_TurnRight(void) {
    // Left: stop (IN1=0, IN2=0)
    GPIO_ResetBits(GPIOC, GPIO_Pin_10);
    GPIO_ResetBits(GPIOC, GPIO_Pin_0);

    // Right: backward (IN3=1, IN4=0)
    GPIO_SetBits  (GPIOB, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Motor_Stop(void) {
    // Left stop
    GPIO_ResetBits(GPIOC, GPIO_Pin_10 | GPIO_Pin_0);
    // Right stop
    GPIO_ResetBits(GPIOB, GPIO_Pin_7 | GPIO_Pin_8);
}

