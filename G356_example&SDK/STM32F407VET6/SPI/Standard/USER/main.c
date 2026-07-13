#include "stm32f4xx.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "spi.h"
#include "g356_driver.h"
#include <stdio.h>

static void SystemClock_Config_168MHz_HSI(void)
{
    RCC_DeInit();
    RCC_HSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET) {
    }

    RCC_PLLConfig(RCC_PLLSource_HSI, 16, 336, 2, 7);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {
    }

    FLASH_SetLatency(FLASH_Latency_5);
    FLASH_PrefetchBufferCmd(ENABLE);
    FLASH_InstructionCacheCmd(ENABLE);
    FLASH_DataCacheCmd(ENABLE);

    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div4);
    RCC_PCLK2Config(RCC_HCLK_Div2);
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08) {
    }

    SystemCoreClockUpdate();
}

int main(void)
{
    uint8_t frame[G356_FRAME_LEN];
    G356_Data_t data;

    SystemClock_Config_168MHz_HSI();
    NVIC_PriorityGroupConfig_2();
    delay_init();
    USART1_Debug_Init(115200);
    SPI1_G356_Init();

    printf("\r\nG356 STM32F407 Standard SPI example\r\n");

    while (1) {
        if (SPI1_G356_ReadPacket(frame, G356_FRAME_LEN) && G356_ParseData(frame, &data)) {
            printf("ACC[g] %.3f %.3f %.3f  GYRO[dps] %.2f %.2f %.2f  RPY[deg] %.2f %.2f %.2f  T %.2f\r\n",
                   data.accel_x, data.accel_y, data.accel_z,
                   data.gyro_x, data.gyro_y, data.gyro_z,
                   data.roll, data.pitch, data.yaw, data.temp);
        }
        delay_ms(20);
    }
}
