#include "inc/hw_config.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"
#include "misc.h"

extern volatile uint32_t ADC_Value[1];

static void BUZZER_PinToGPIO_High(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_6); // Low-trigger OFF
}

static void BUZZER_PinToAF(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void RCC_Configure(void) {
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_ADC1  |
        RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOD |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_AFIO  |
        RCC_APB2Periph_USART1,
        ENABLE
    );

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2   |
        RCC_APB1Periph_TIM3   |   // BUZZER PWM
        RCC_APB1Periph_USART2,
        ENABLE
    );
}

void GPIO_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1) ADC (PC0)
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 2) Button (PA0) Pull-up
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3) BUZZER (PC6) - 기본은 OFF(High)
    BUZZER_PinToGPIO_High();

    // 4) USART1 (PA9 TX / PA10 RX)
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 5) USART2 (PD5/6 remap)
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
}

void USART1_Init(void) {
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate            = 9600;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

void USART2_Init(void) {
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate            = 9600;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

void NVIC_Configure(void) {
    NVIC_InitTypeDef NVIC_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // TIM2 IRQ (1s tick)
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // EXTI0 (PA0 Button)
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);

    EXTI_InitStructure.EXTI_Line    = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // USART1
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // USART2
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void DMA_Configure(void) {
    DMA_InitTypeDef DMA_InitStruct;

    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)&ADC_Value[0];
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStruct.DMA_BufferSize         = 1;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Word;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_High;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;

    DMA_Init(DMA1_Channel1, &DMA_InitStruct);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void ADC_Configure(void) {
    ADC_InitTypeDef ADC_InitStruct;

    ADC_InitStruct.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStruct.ADC_ScanConvMode       = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStruct.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStruct.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfChannel       = 1;

    ADC_Init(ADC1, &ADC_InitStruct);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_28Cycles5);

    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    ADC_DMACmd(ADC1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void USART2_SendString(const char* str) {
    while (*str) {
        USART_SendData(USART2, (uint16_t)(*str++));
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

/* =========================
   BUZZER: PC6 (TIM3_CH1 Full Remap)
   Low-trigger: LOW during pulse = ON
   ========================= */
void BUZZER_Configure(void) {
    // TIM3 Full Remap: CH1->PC6, CH2->PC7, CH3->PC8, CH4->PC9
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);

    // 기본은 정지
    BUZZER_Stop();
}

void BUZZER_Start(uint16_t freq_hz) {
    if (freq_hz < 50) freq_hz = 50;

    // PC6를 AF로
    BUZZER_PinToAF();

    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef oc;

    TIM_Cmd(TIM3, DISABLE);
    TIM_DeInit(TIM3);

    // (가정) TIM3 clock ~= 72MHz -> 1MHz tick 만들기
    // tick = 1MHz => PSC = 72-1
    tb.TIM_Prescaler         = 72 - 1;
    tb.TIM_CounterMode       = TIM_CounterMode_Up;
    tb.TIM_ClockDivision     = 0;
    tb.TIM_RepetitionCounter = 0;

    // 1MHz / freq = period_counts
    uint32_t period = (1000000UL / (uint32_t)freq_hz);
    if (period < 2) period = 2;
    tb.TIM_Period = (uint16_t)(period - 1);

    TIM_TimeBaseInit(TIM3, &tb);

    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;

    // 50% duty
    oc.TIM_Pulse = (uint16_t)(period / 2);

    // Low-trigger: active level = LOW during pulse
    oc.TIM_OCPolarity = TIM_OCPolarity_Low;

    TIM_OC1Init(TIM3, &oc);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void BUZZER_Stop(void) {
    // PWM 끄고 PC6를 High로 고정(OFF)
    TIM_Cmd(TIM3, DISABLE);
    TIM_DeInit(TIM3);
    BUZZER_PinToGPIO_High();
}
