#include "stm32f10x.h"

/*
 * CODE DESCRIPTION :
 *     GPIO Operation
 *     - LED: PD2, PD3, PD4, PD7
 *     - KEY: PC4, PB10, PC13, PA0
*/


// Modify the register starting address
#define RCC_APB2ENR (*(volatile unsigned int *) (0x40021000 + 0x18))
//                                               0x40021018

#define GPIOA_CRL (*(volatile unsigned int *) (0x00000000))
#define GPIOA_IDR (*(volatile unsigned int *) (0x00000000))
 
#define GPIOB_CRH (*(volatile unsigned int *) (0x00000000))
#define GPIOB_IDR (*(volatile unsigned int *) (0x00000000))

#define GPIOC_CRL (*(volatile unsigned int *) (0x00000000))
#define GPIOC_CRH (*(volatile unsigned int *) (0x00000000))
#define GPIOC_IDR (*(volatile unsigned int *) (0x00000000))

#define GPIOD_CRL (*(volatile unsigned int *) (0x00000000))
#define GPIOD_BSRR (*(volatile unsigned int *) (0x00000000))


void RCCEnable() // A, B, C, D Port Enable
{
    // PA
    RCC_APB2ENR |= 0x00000000;

    // PB
    RCC_APB2ENR |= 0x00000000;

    // PC
    RCC_APB2ENR |= 0x00000000;

    // PD
    RCC_APB2ENR |= 0x00000000;

}

void PortConfiguration()
{
    // PC4 (Input with pull-up / pull-down) (KEY1)
    GPIOC_CRL &= 0x00000000; // reset
    GPIOC_CRL |= 0x00000000; // set

    // PB10 (Input with pull-up / pull-down) (KEY2)
    GPIOB_CRH &= 0x00000000; // reset
    GPIOB_CRH |= 0x00000000; // set

    // PC13 (Input with pull-up / pull-down) (KEY3)
    GPIOC_CRH &= 0x00000000; // reset
    GPIOC_CRH |= 0x00000000; // set

    // PA0 (Input with pull-up / pull-down) (KEY4)
    GPIOA_CRL &= 0x00000000; // reset
    GPIOA_CRL |= 0x00000000; // set



    // PD2 (push-pull, 50MHz) (LED1)
    GPIOD_CRL &= 0x00000000; // reset
    GPIOD_CRL |= 0x00000000; // set

    // PD3 (push-pull, 50MHz) (LED2)
    GPIOD_CRL &= 0x00000000; // reset
    GPIOD_CRL |= 0x00000000; // set

    // PD4 (push-pull, 50MHz) (LED3)
    GPIOD_CRL &= 0x00000000; // reset
    GPIOD_CRL |= 0x00000000; // set

    // PD7 (push-pull, 50MHz) (LED4)
    GPIOD_CRL &= 0x00000000; // reset
    GPIOD_CRL |= 0x00000000; // set

}

void HandleButtonInput()
{
    // KEY1 is pressed (PC4)
    if ( !(GPIOC_IDR & 0x00000000) ) // Pull up
    {
        // PD2, PD3 On
        GPIOD_BSRR |= 0x00000000;
        GPIOD_BSRR |= 0x00000000;
    }

    // KEY2 is Pressed (PB10)
    if ( !(GPIOB_IDR & 0x00000000) ) // Pull up
    {
        // PD2, PD3 Off
        GPIOD_BSRR |= 0x00000000;
        GPIOD_BSRR |= 0x00000000;
    }

    // KEY3 is Pressed (PC13)
    if ( !(GPIOC_IDR & 0x00000000) ) // Pull up
    {
        // PD4, PD7 On
        GPIOD_BSRR |= 0x00000000;
        GPIOD_BSRR |= 0x00000000;
    }

    // KEY4 is Pressed (PA0)
    if ( !(GPIOA_IDR & 0x00000000) ) // Pull up
    {
        // PD4, PD7 Off
        GPIOD_BSRR |= 0x00000000;
        GPIOD_BSRR |= 0x00000000;
    }
}

int main(void)
{
    RCCEnable();
    PortConfiguration();
    /* turn off the all LEDs */
    // Set: LED turn off, Reset: LED turn on
    // Port D BSRR
    GPIOD_BSRR |= 0x9C; // GPIOD_BSRR |= 0x80 | 0x10 | 0x08 | 0x04; or GPIOD_BSRR |= (1 << 7) | (1 << 4) | (1 << 3) | (1 << 2); 

    while (1)
    {
        HandleButtonInput();
    }
 
    return 0;
}