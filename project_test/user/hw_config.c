#include "inc/hw_config.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "misc.h"

extern volatile uint32_t ADC_Value[1];

// [추가] 미세 시간 지연 함수 (초음파 센서용)
static void Delay_us(uint32_t us) {
    us *= 12;
    while (us--) {
        __NOP();
    }
}

void RCC_Configure(void) {
    // ADC1, GPIOA, GPIOC, GPIOD, GPIOB, AFIO, USART1 클럭 활성화
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_USART2, ENABLE);
}

void GPIO_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 빗물 감지 센서 (Analog Input) - PA1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 2. 알람 정지 버튼 (PA0)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. USART1 (PA9: Tx, PA10: Rx)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. USART2 (PD5: Tx, PD6: Rx)
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // 5. 부저 (PB0)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_0); // 부저 끄기

    // 6. 터치센서 (PC1)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// [추가] 초음파 센서 설정 함수
void Ultrasonic_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1번 센서 (왼쪽): TRIG=PA4, ECHO=PA5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; // TRIG
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; // ECHO
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 2번 센서 (정면): TRIG=PA6, ECHO=PA7
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; // TRIG
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7; // ECHO
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3번 센서 (오른쪽): TRIG=PB10, ECHO=PB11
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // TRIG
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; // ECHO
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Trig 핀 초기화 (Low)
    GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_6);
    GPIO_ResetBits(GPIOB, GPIO_Pin_10);
}

// [추가] 거리 측정 함수 (cm 단위 리턴)
// sensor_id: 1(왼쪽), 2(정면), 3(오른쪽)
uint32_t Get_Ultrasonic_Dist(uint8_t sensor_id) {
    GPIO_TypeDef* TRIG_PORT;
    uint16_t TRIG_PIN;
    GPIO_TypeDef* ECHO_PORT;
    uint16_t ECHO_PIN;

    // 센서 선택
    switch(sensor_id) {
        case 1: // PA4, PA5
            TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_4;
            ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_5;
            break;
        case 2: // PA6, PA7
            TRIG_PORT = GPIOA; TRIG_PIN = GPIO_Pin_6;
            ECHO_PORT = GPIOA; ECHO_PIN = GPIO_Pin_7;
            break;
        case 3: // PB10, PB11
            TRIG_PORT = GPIOB; TRIG_PIN = GPIO_Pin_10;
            ECHO_PORT = GPIOB; ECHO_PIN = GPIO_Pin_11;
            break;
        default: return 0;
    }

    // 1. Trigger 신호 발생 (10us High)
    GPIO_SetBits(TRIG_PORT, TRIG_PIN);
    Delay_us(10);
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    // 2. Echo 응답 대기
    uint32_t timeout = 100000;
    while (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == RESET) {
        if (timeout-- == 0) return 0;
    }

    // 3. Pulse 폭 측정
    uint32_t count = 0;
    while (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == SET) {
        count++;
        Delay_us(1);
        if (count > 50000) break;
    }

    // 4. 거리 계산 (대략적인 보정치 58 사용)
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

    // TIM2
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // EXTI0 (버튼)
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

    // EXTI1 (터치센서)
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

    // USART1, 2
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
