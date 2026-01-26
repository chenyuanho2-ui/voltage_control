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
#define MCP4725_ADDR (0x60 << 1)

// 串口接收相关变量
#define RX_BUFFER_SIZE 20
uint8_t rx_data;               // 接收单个字符
uint8_t rx_buffer[RX_BUFFER_SIZE]; // 接收缓冲字符串
uint8_t rx_index = 0;          // 缓冲区索引
uint8_t cmd_received = 0;      // 接收完成标志位

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 设置 MCP4725 输出电压 (0-4095)
// 对应手册中的 Fast Mode Write Command
void MCP4725_SetOutput(uint16_t value) {
    uint8_t i2c_data[2];
    
    // 限制最大值，防止溢出
    if (value > 4095) value = 4095;

    // 数据格式：
    // 第1字节：0 0 PD1 PD0 D11 D10 D9 D8 (PD=00为正常模式)
    // 第2字节：D7 D6 D5 D4 D3 D2 D1 D0
    i2c_data[0] = (uint8_t)((value >> 8) & 0x0F); 
    i2c_data[1] = (uint8_t)(value & 0xFF);

    // 发送 I2C 数据
    if (HAL_I2C_Master_Transmit(&hi2c1, MCP4725_ADDR, i2c_data, 2, 100) != HAL_OK) {
        printf("Error: I2C Transmit Failed\r\n");
    }
}

// 串口接收中断回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_data == '\n' || rx_data == '\r') { // 收到换行符，认为指令结束
            if (rx_index > 0) { // 确保有数据
                rx_buffer[rx_index] = '\0'; // 添加字符串结束符
                cmd_received = 1; // 标记指令已接收
                rx_index = 0; // 重置索引
            }
        } else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_data; // 存入缓冲区
            }
        }
        // 重新开启接收中断
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// 重定向 printf 到串口，方便向电脑发送信息
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
        cmd_received = 0; // 清除标志位
        
        int speed = atoi((char*)rx_buffer); // 将字符串转换为整数
        
        if (speed < 0) {
            printf("Error: Speed cannot be negative.\r\n");
        } 
        else if (speed > 100) {
            printf("Error: Speed %d out of range! Max is 100.\r\n", speed);
        } 
        else {
            // 计算 DAC 数值 (0-100 映射到 0-4095)
            // 4095 / 100 = 40.95
            uint16_t dac_val = (uint16_t)(speed * 40.95);
            
            // 设置电压
            MCP4725_SetOutput(dac_val);
            
            // 计算理论电压便于显示
            float voltage = (float)speed / 100.0f * 5.0f;
            printf("Set Speed: %d%%, DAC Val: %d, Voltage: %.2fV\r\n", speed, dac_val, voltage);
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
