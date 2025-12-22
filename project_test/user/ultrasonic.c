#include "inc/ultrasonic.h"
#include "stm32f10x_gpio.h"

static void Delay_us(uint32_t us) {
    volatile uint32_t count = us * 12;
    while (count--) {
        __NOP();
    }
}

void Ultrasonic_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // PA4(Trig), PA5(Echo) - Left
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA6(Trig), PA7(Echo) - Center
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PB10(Trig), PB11(Echo) - Right
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_6);
    GPIO_ResetBits(GPIOB, GPIO_Pin_10);
}

uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id) {
    GPIO_TypeDef* TRIG_PORT;
    uint16_t TRIG_PIN;
    GPIO_TypeDef* ECHO_PORT;
    uint16_t ECHO_PIN;

    switch(sensor_id) {
        case 1: TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_4; ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_5; break;
        case 2: TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_6; ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_7; break;
        case 3: TRIG_PORT = GPIOB; TRIG_PIN = GPIO_Pin_10; ECHO_PORT = GPIOB; ECHO_PIN = GPIO_Pin_11; break;
        default: return 0;
    }

    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);
    Delay_us(5);
    GPIO_SetBits(TRIG_PORT, TRIG_PIN);
    Delay_us(15);
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    uint32_t timeout = 50000;
    while (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == RESET) {
        if (timeout-- == 0) return 0;
    }

    uint32_t count = 0;
    while (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == SET) {
        count++;
        if (count > 100000) break;
    }

    return count / 58;
}

