/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "g356_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ==========================================================================
// G356 UART Receiver Demo
//
// This file only demonstrates how to call the reusable G356 driver
// (g356_driver.h/.c). If you're integrating G356 into your own product,
// you only need to copy g356_driver.h/.c — the statistics/pretty-printing
// below are demo-only and can be deleted.
//
// Wiring and protocol details: see g356_driver.h and ../README.md.
// ==========================================================================

// ==========================================
// 调试串口输出辅助函数 (避免依赖大体积的 C 库 printf 及其浮点支持)
// ==========================================
static void G356_UART_PrintString(const char *str)
{
    while (*str) {
        HAL_UART_Transmit(&huart1, (uint8_t *)str, 1, 10);
        str++;
    }
}

static void G356_UART_PrintUint(uint32_t val)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (val == 0) {
        G356_UART_PrintString("0");
        return;
    }
    while (val > 0) {
        i--;
        buf[i] = (val % 10) + '0';
        val /= 10;
    }
    G356_UART_PrintString(&buf[i]);
}

static void G356_UART_PrintFloat(float val, int decimals)
{
    // 处理负数
    if (val < 0.0f) {
        G356_UART_PrintString("-");
        val = -val;
    }

    // 四舍五入处理
    float rounding = 0.5f;
    for (int d = 0; d < decimals; d++) {
        rounding /= 10.0f;
    }
    val += rounding;

    // 整数部分
    uint32_t int_part = (uint32_t)val;
    G356_UART_PrintUint(int_part);

    // 小数部分
    if (decimals > 0) {
        G356_UART_PrintString(".");
        float frac = val - (float)int_part;
        for (int d = 0; d < decimals; d++) {
            frac *= 10.0f;
            uint32_t digit = (uint32_t)frac;
            G356_UART_PrintUint(digit);
            frac -= (float)digit;
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1000); // 等待 G356 模块内部系统彻底启动并稳定

  G356_UART_PrintString("\r\n");
  G356_UART_PrintString("=========================================\r\n");
  G356_UART_PrintString("   G356 AHRS Module UART Receiver Demo   \r\n");
  G356_UART_PrintString("   MCU: STM32F407VET6 | Baudrate: 115200 \r\n");
  G356_UART_PrintString("   Communication: USART2 (115200, 8N1)   \r\n");
  G356_UART_PrintString("=========================================\r\n");
  G356_UART_PrintString("Starting data reading...\r\n");

  uint8_t rx_frame_buf[G356_FRAME_SIZE]; // 校验通过的完整帧缓冲区
  G356_Data_t g356_data;                 // 解析后的姿态数据结构体
  uint32_t valid_count = 0;              // 校验通过的帧计数
  uint32_t invalid_count = 0;            // 校验失败的帧计数

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // G356 以约 100Hz 持续往外发送遥测帧，这里逐字节阻塞接收并喂给状态机
    // (UART 是连续字节流，没有 SPI 那种片选信号天然分帧，所以用状态机
    //  自己识别帧头、定长收集、校验)
    uint8_t byte;
    if (HAL_UART_Receive(&huart2, &byte, 1, HAL_MAX_DELAY) != HAL_OK) {
        continue;
    }
    G356_FrameStatus status = G356_FeedByte(byte, rx_frame_buf);

    if (status == G356_FRAME_VALID) {
        valid_count++;

        G356_ParseData(rx_frame_buf, &g356_data);

        // 每成功读取 20 次 (约 200ms) 打印一次数据，防止串口拥堵
        if (valid_count % 20 == 0) {
            G356_UART_PrintString("ANG[R:");
            G356_UART_PrintFloat(g356_data.roll, 2);
            G356_UART_PrintString(", P:");
            G356_UART_PrintFloat(g356_data.pitch, 2);
            G356_UART_PrintString(", Y:");
            G356_UART_PrintFloat(g356_data.yaw, 2);

            G356_UART_PrintString("] deg | ACC[");
            G356_UART_PrintFloat(g356_data.accel_x, 3);
            G356_UART_PrintString(", ");
            G356_UART_PrintFloat(g356_data.accel_y, 3);
            G356_UART_PrintString(", ");
            G356_UART_PrintFloat(g356_data.accel_z, 3);

            G356_UART_PrintString("] g | GYR[");
            G356_UART_PrintFloat(g356_data.gyro_x, 1);
            G356_UART_PrintString(", ");
            G356_UART_PrintFloat(g356_data.gyro_y, 1);
            G356_UART_PrintString(", ");
            G356_UART_PrintFloat(g356_data.gyro_z, 1);

            G356_UART_PrintString("] dps | Temp:");
            G356_UART_PrintFloat(g356_data.temp, 1);
            G356_UART_PrintString(" C\r\n");
        }
    } else if (status == G356_FRAME_INVALID) {
        invalid_count++;

        // 每失败 100 次打印一次错误警告，方便排查接线/波特率问题
        if (invalid_count % 100 == 0) {
            G356_UART_PrintString("[Warning] Frame validation failed! Valid: ");
            G356_UART_PrintUint(valid_count);
            G356_UART_PrintString(", Invalid: ");
            G356_UART_PrintUint(invalid_count);
            G356_UART_PrintString("\r\n");
        }
    }
    // status == G356_FRAME_PENDING: 还没收完一整帧，继续读下一个字节
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *        HSI(16MHz, internal) -> PLL(M=16,N=336,P=2,Q=7) -> SYSCLK 168MHz,
  *        APB1 42MHz, APB2 84MHz. Deliberately HSI-based (no external HSE
  *        crystal dependency) so this demo runs unmodified regardless of
  *        which HSE crystal (or none) is fitted on the user's particular
  *        F407VET6 minimum-system-board — HSI_VALUE is a fixed, correct
  *        16MHz constant, whereas HSE_VALUE in stm32f4xx_hal_conf.h defaults
  *        to 25MHz and silently mismatching it against a board's actual HSE
  *        crystal miscalculates every HAL_RCC_GetPCLKxFreq()-derived value
  *        (UART baud rate, HAL_Delay() tick) without any error being raised.
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
