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
#include <string.h>
#include "usart.h"
#include "gpio.h"
#include "flash.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "boot.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define KEY_PRESSED() HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET
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
void JumpToApplication(uint32_t appAddress)
{
  // 检查应用程序的堆栈指针是否有效(即是否在 SRAM 的范围内)
  if(((*(__IO uint32_t*)appAddress) & 0x2FFE0000) == 0x20000000)
  {
    // 定义一个函数指针类型，用于指向应用程序的复位处理程序
    typedef void (*pFunction)(void);

    uint32_t app_sp = *(__IO uint32_t*)appAddress; // 获取应用程序的堆栈指针
    uint32_t app_entry = *(__IO uint32_t*)(appAddress + 4); // 获取应用程序的复位处理程序地址
    // 设置函数指针指向应用程序的复位处理程序
    pFunction app_reset_handler = (pFunction)app_entry;
    // 关闭中断 关闭 SysTick 定时器
    __disable_irq();
    SysTick->CTRL = 0; // 禁用 SysTick 定时器
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    // 设置向量表偏移寄存器为应用程序的起始地址
    SCB->VTOR = appAddress;
    __set_MSP(app_sp); // 设置主堆栈指针

    app_reset_handler(); // 跳转到应用程序的复位处理程序

    return;
  }
}

HAL_StatusTypeDef CopyFirmware(void)
{
  uint32_t src_addr = UPGRADE_START_ADDR;
  uint32_t size = UPGRADE_SIZE;
  
  uint32_t dest_addr = APP_START_ADDR;

  FLASH_EraseInitTypeDef eraseInit = {0};
  uint32_t sectorError = 0xFFFFFFFFU;
  HAL_StatusTypeDef status = HAL_OK;

  HAL_FLASH_Unlock();
  // 配置擦除应用区
  eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
  eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  eraseInit.Sector = FLASH_SECTOR_2;
  eraseInit.NbSectors = 6;  // 扇区2~7

  // 一次性擦除
  if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK)
  {
    UART1_SendString("Erase app area failed!\r\n");
    status = HAL_ERROR;
    goto cleanup;
  }

  // 按32位复制固件
  for (uint32_t i = 0; i < size; i += 4U)
  {
    uint32_t data;
    // 从升级区读取32位数据
    memcpy(&data, (const void *)(src_addr + i), sizeof(data));
    // 写入应用区
    if (HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_WORD,
        dest_addr + i,
        data) != HAL_OK)
    {
      UART1_SendString("Write app area failed!\r\n");
      status = HAL_ERROR;
      goto cleanup;
    }

    // 每复制256字节翻转一次LED
    if ((i % 256U) == 0U)
    {
      HAL_GPIO_TogglePin(GPIOF, LED_R_Pin);
    }
  }
  cleanup:
    // 无论成功失败，都重新锁定Flash
    if (HAL_FLASH_Lock() != HAL_OK)
    {
        status = HAL_ERROR;
    }

    if (status == HAL_OK)
    {
        UART1_SendString("Firmware copy complete!\r\n");
    }
    return status;
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
  /* USER CODE BEGIN 2 */
  for (int i = 0; i < 2; i++)
  {
    //HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
    HAL_GPIO_WritePin(GPIOF, LED_R_Pin, GPIO_PIN_SET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOF, LED_R_Pin, GPIO_PIN_RESET);
    HAL_Delay(500);
  }

  uint8_t need_upgrade = 0;
  /* 按键检查是否需要升级 */
  if (KEY_PRESSED())
  {
    need_upgrade = 1;
  }
  /* 进入升级模式 */ 
  if(need_upgrade)
  {
    /* 按键升级亮绿灯 */
    HAL_GPIO_WritePin(GPIOF, LED_G_Pin, GPIO_PIN_RESET);

    while(1)
    {
      /* 等待命令，接收一个字节 */
      uint8_t cmd = UART1_ReceiveByte(HAL_MAX_DELAY);

      switch(cmd)
      {
        case'1':
        /* 下载固件到升级缓冲区 */
          FirmwareDownload();
          /* 复制固件到应用区 */
          if(CopyFirmware() == HAL_OK)
          {
            HAL_Delay(1000);
            /* 重启 */
            NVIC_SystemReset();
          }
          else
          {
            break;
          }

        case '2':
          /* 跳转到应用程序 */ 
          JumpToApplication(APP_START_ADDR);
          break;

        case '3':
          HAL_Delay(100);
          NVIC_SystemReset();
          break;

        default:
          HAL_GPIO_TogglePin(GPIOF, LED_R_Pin);
          break;
      }
    }
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
#ifdef USE_FULL_ASSERT
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
