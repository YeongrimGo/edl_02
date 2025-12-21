#include "inc/hw_config.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "misc.h"

extern volatile uint32_t ADC_Value[1];

static void Delay_us(uint32_t us) {
    volatile uint32_t count = us * 12;
    while (count--) { __NOP(); }
}

void RCC_Configure(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_USART2, ENABLE);
}

void GPIO_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // PA 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PD (USART2 Remap)
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOD, &GPIO_InitStructure);

    // PB (Buzzer)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_0);

    // PC (Sensor Input)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void Ultrasonic_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_6);
    GPIO_ResetBits(GPIOB, GPIO_Pin_10);
}

void Motor_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // [수정 완료] PB9 -> PB12로 변경
    // IN1(PB5), IN2(PB12), IN3(PB7), IN4(PB8)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    Motor_Stop();
}

// --- 모터 제어 함수 수정됨 (PB9 -> PB12) ---

void Motor_Forward(void) {
    // 왼쪽 바퀴 전진 (IN1=0, IN2=1)
    GPIO_ResetBits(GPIOB, GPIO_Pin_5);
    GPIO_SetBits(GPIOB, GPIO_Pin_12); // [수정] PB12
    // 오른쪽 바퀴 전진 (IN3=0, IN4=1)
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_SetBits(GPIOB, GPIO_Pin_8);
}

void Motor_Backward(void) {
    // 왼쪽 바퀴 후진 (IN1=1, IN2=0)
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // [수정] PB12
    // 오른쪽 바퀴 후진 (IN3=1, IN4=0)
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Motor_TurnLeft(void) {
    // 왼쪽: 후진
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // [수정] PB12

    // 오른쪽: 전진
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_SetBits(GPIOB, GPIO_Pin_8);
}

void Motor_TurnRight(void) {
    // 왼쪽: 전진
    GPIO_ResetBits(GPIOB, GPIO_Pin_5);
    GPIO_SetBits(GPIOB, GPIO_Pin_12); // [수정] PB12

    // 오른쪽: 후진
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Motor_Stop(void) {
    // [수정] PB12 포함하여 정지
    GPIO_ResetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_7 | GPIO_Pin_8);
}

uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id) {
    GPIO_TypeDef* TRIG_PORT;
    uint16_t TRIG_PIN;
    GPIO_TypeDef* ECHO_PORT;
    uint16_t ECHO_PIN;

    switch(sensor_id) {
        case 1:
            TRIG_PORT = GPIOB; TRIG_PIN = GPIO_Pin_10; ECHO_PORT = GPIOB; ECHO_PIN = GPIO_Pin_11;
            break;
        case 2:
            TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_6; ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_7;
            break;
        case 3:
            TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_4; ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_5;
            break;
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

void USART1_Init(void) {
    USART_InitTypeDef USART_InitStructure;
    USART_Cmd(USART1, ENABLE);
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

void USART2_Init(void) {
    USART_InitTypeDef USART_InitStructure;
    USART_Cmd(USART2, ENABLE);
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

void NVIC_Configure(void) {
    NVIC_InitTypeDef NVIC_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource1);
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void DMA_Configure(void) {
   DMA_InitTypeDef DMA_InitStruct;
   DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
   DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)&ADC_Value[0];
   DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;
   DMA_InitStruct.DMA_BufferSize = 1;
   DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
   DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Disable;
   DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
   DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
   DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
   DMA_InitStruct.DMA_Priority = DMA_Priority_High;
   DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;
   DMA_Init(DMA1_Channel1, &DMA_InitStruct);
   DMA_Cmd(DMA1_Channel1, ENABLE);
}

void ADC_Configure(void) {
   ADC_InitTypeDef ADC_InitStruct;
   ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;
   ADC_InitStruct.ADC_ScanConvMode = DISABLE;
   ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
   ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
   ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
   ADC_InitStruct.ADC_NbrOfChannel = 1;
   ADC_Init(ADC1, &ADC_InitStruct);

   ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5);

   ADC_Cmd(ADC1, ENABLE);
   ADC_ResetCalibration(ADC1);
   while (ADC_GetResetCalibrationStatus(ADC1));
   ADC_StartCalibration(ADC1);
   while (ADC_GetCalibrationStatus(ADC1));

   ADC_SoftwareStartConvCmd(ADC1, ENABLE);
   ADC_DMACmd(ADC1, ENABLE);
}

void USART2_SendString(const char* str) {
    while (*str) {
        USART_SendData(USART2, *str++);
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

void Sensor_Mode_WaitRain(void) {
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStructure);
}

void Sensor_Mode_Reset(void) {
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    EXTI_ClearITPendingBit(EXTI_Line1);
}
