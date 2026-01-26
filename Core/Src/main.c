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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "calibration.h" // 引入校准模块
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

// MCP4725 I2C地址 (A0接地时为0x60，HAL库需要左移一位变成0xC0)
// MCP4725 I2C 地址 (如果你用的是红色模块，通常 A0 接地，地址是 0x60)
// HAL 库需要左移一位，即 0x60 << 1 = 0xC0
#define MCP4725_ADDR (0x60 << 1)

// 参考 Adafruit_MCP4725.h 中的定义
#define MCP4725_CMD_WRITEDAC        0x40  // 写 DAC 寄存器命令
#define MCP4725_CMD_WRITEDACEEPROM  0x60  // 写 DAC 寄存器和 EEPROM 命令

// 串口接收相关变量
#define RX_BUFFER_SIZE 20
uint8_t rx_data;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;
uint8_t cmd_received = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 移植自 Adafruit_MCP4725::setVoltage
// writeEEPROM: 1=写入EEPROM(掉电保存), 0=仅写入DAC(掉电不保存)
void MCP4725_SetVoltage(uint16_t output, uint8_t writeEEPROM) {
    uint8_t packet[3];

    // 限制范围 0-4095
    if (output > 4095) output = 4095;

    // 1. 设置命令字节
    if (writeEEPROM) {
        packet[0] = MCP4725_CMD_WRITEDACEEPROM;
    } else {
        packet[0] = MCP4725_CMD_WRITEDAC;
    }

    // 2. 计算数据字节 (完全参考 Adafruit 的位操作逻辑)
    // output / 16 等同于 output >> 4，取高8位
    packet[1] = (uint8_t)(output / 16); 
    // (output % 16) << 4 等同于 (output & 0x0F) << 4，取低4位并左移
    packet[2] = (uint8_t)((output % 16) << 4);

    // 3. 发送 3 个字节
    if (HAL_I2C_Master_Transmit(&hi2c1, MCP4725_ADDR, packet, 3, 100) != HAL_OK) {
        printf("Error: I2C Transmit Failed (NACK)\r\n");
    }
}

// 串口接收回调 (保持不变)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_data == '\n' || rx_data == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                cmd_received = 1;
                rx_index = 0;
            }
        } else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_data;
            }
        }
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// printf 重定向 (保持不变)
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
    return ch;
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

printf("System Started. Please enter speed (0-100):\r\n");
  
  // 开启串口接收中断
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  
	  if (cmd_received) {
        cmd_received = 0;
        
        // 解析用户输入的目标转速 (float支持小数，例如输入 50.5)
        float target_speed = atof((char*)rx_buffer); 
        
        if (target_speed < 0) {
            printf("Error: Speed cannot be negative.\r\n");
        } 
        else if (target_speed > 100) {
            printf("Error: Speed %.2f out of range! Max is 100.\r\n", target_speed);
        } 
        else {
            // --- 核心修改：调用校准函数 ---
            // 比如你想转 50，校准函数会算出可能只需要输入 48.5
            float corrected_cmd = Calibration_GetCorrectedValue(target_speed);
            
            // 计算 DAC 数值 (0-100 映射到 0-4095)
            // 使用修正后的值来计算电压
            uint16_t dac_val = (uint16_t)(corrected_cmd * 40.95f);
            
            // 限制 DAC 范围防止溢出
            if(dac_val > 4095) dac_val = 4095;

            // 设置电压
            MCP4725_SetVoltage(dac_val, 0); // 0表示不写入EEPROM
            
            // 显示信息
            // 理论电压是给用户看的，所以用 target_speed
            // 实际输出是给泵看的，用了 corrected_cmd
            printf("Target: %.2f%%, Corrected CMD: %.2f%%, DAC: %d\r\n", 
                   target_speed, corrected_cmd, dac_val);
        }
    }
}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
