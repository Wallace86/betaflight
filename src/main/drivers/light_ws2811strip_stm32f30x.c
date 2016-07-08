/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#include <platform.h>

#include "io.h"
#include "nvic.h"

#include "common/color.h"
#include "drivers/light_ws2811strip.h"
#include "dma.h"
#include "rcc.h"
#include "timer.h"

#ifdef LED_STRIP

#ifndef WS2811_PIN
#define WS2811_PIN                      PB8 // TIM16_CH1
#define WS2811_TIMER                    TIM16
#define WS2811_DMA_CHANNEL              DMA1_Channel3
#define WS2811_DMA_HANDLER_IDENTIFER    DMA1_CH3_HANDLER
#endif

static IO_t ws2811IO = IO_NONE;
bool ws2811Initialised = false;

static void WS2811_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor) {
    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TCIF)) {
        ws2811LedDataTransferInProgress = 0;
        DMA_Cmd(descriptor->channel, DISABLE);
        DMA_CLEAR_FLAG(descriptor, DMA_IT_TCIF);
    }
}

void ws2811LedStripHardwareInit(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    uint16_t prescalerValue;
    
    ws2811IO = IOGetByTag(IO_TAG(WS2811_PIN));
    /* GPIOA Configuration: TIM5 Channel 1 as alternate function push-pull */
    IOInit(ws2811IO, OWNER_SYSTEM, RESOURCE_OUTPUT);
    IOConfigGPIOAF(ws2811IO, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP), timerGPIOAF(WS2811_TIMER));
    
    RCC_ClockCmd(timerRCC(WS2811_TIMER), ENABLE);

    /* Compute the prescaler value */
    prescalerValue = (uint16_t) (SystemCoreClock / 24000000) - 1;
    /* Time base configuration */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 29; // 800kHz
    TIM_TimeBaseStructure.TIM_Prescaler = prescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(WS2811_TIMER, &TIM_TimeBaseStructure);

    /* PWM1 Mode configuration */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(WS2811_TIMER, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(WS2811_TIMER, TIM_OCPreload_Enable);


    TIM_CtrlPWMOutputs(WS2811_TIMER, ENABLE);

    /* configure DMA */
    /* DMA1 Channel Config */
    DMA_DeInit(WS2811_DMA_CHANNEL);

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&WS2811_TIMER->CCR1;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ledStripDMABuffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = WS2811_DMA_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

    DMA_Init(WS2811_DMA_CHANNEL, &DMA_InitStructure);

    TIM_DMACmd(WS2811_TIMER, TIM_DMA_CC1, ENABLE);

    DMA_ITConfig(WS2811_DMA_CHANNEL, DMA_IT_TC, ENABLE);

    dmaSetHandler(WS2811_DMA_HANDLER_IDENTIFER, WS2811_DMA_IRQHandler, NVIC_PRIO_WS2811_DMA, 0);

    ws2811Initialised = true;
    setStripColor(&hsv_white);
    ws2811UpdateStrip();
}

void ws2811LedStripDMAEnable(void)
{
    if (!ws2811Initialised)
        return;
    
    DMA_SetCurrDataCounter(WS2811_DMA_CHANNEL, WS2811_DMA_BUFFER_SIZE);  // load number of bytes to be transferred
    TIM_SetCounter(WS2811_TIMER, 0);
    TIM_Cmd(WS2811_TIMER, ENABLE);
    DMA_Cmd(WS2811_DMA_CHANNEL, ENABLE);
}

#endif
