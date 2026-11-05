/**
 * stm32f4xx_it.c — interrupt handlers minimal untuk test SENSOR.
 * SysTick -> HAL_IncTick(); DMA2_Stream0 -> ADC1 (scan 4 kanal).
 */

#include "stm32f4xx_hal.h"

extern DMA_HandleTypeDef hdma_adc1;

void NMI_Handler(void)        { for (;;) { } }
void HardFault_Handler(void)  { for (;;) { } }
void MemManage_Handler(void)  { for (;;) { } }
void BusFault_Handler(void)   { for (;;) { } }
void UsageFault_Handler(void) { for (;;) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}
