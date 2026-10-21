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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "flash.h"
#include "iap.h"
#include "firmware.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define KEY_PRESSED() HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
uint8_t rx_data;
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
  const uint32_t app_sp = *(__IO uint32_t*)appAddress;
  const uint32_t app_entry = *(__IO uint32_t*)(appAddress + 4U);
  const uint32_t sram_start = SRAM_BASE;
  const uint32_t sram_end = SRAM_BASE + (128U * 1024U);

  /* 查一下什么意思 */
  if ((app_sp >= sram_start) && (app_sp <= sram_end) &&
      ((app_sp & 0x7U) == 0U) &&
      ((app_entry & 1U) != 0U) &&
      ((app_entry & ~1U) >= APP_ADDRESS) &&
      ((app_entry & ~1U) < (APP_ADDRESS + APP_SIZE)))
  {
    // 定义一个函数指针类型，用于指向应用程序的复位处理程序
    typedef void (*pFunction)(void);

    // 设置函数指针指向应用程序的复位处理程序
    pFunction app_reset_handler = (pFunction)app_entry;
    // 关闭中断 关闭 SysTick 定时器
    __disable_irq();
    SysTick->CTRL = 0; // 禁用 SysTick 定时器
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    UART1_SendString("Valid application found!\r\n");
    // char txBuf[100];

    // snprintf(txBuf, sizeof(txBuf),
    //      "APP SP: 0x%08lX\r\nAPP Entry: 0x%08lX\r\n",
    //      (unsigned long)app_sp,
    //      (unsigned long)app_entry);

    // UART1_SendString(txBuf);  
    // 设置向量表偏移寄存器为应用程序的起始地址
    SCB->VTOR = appAddress;
    __set_MSP(app_sp); // 设置主堆栈指针
    
    app_reset_handler(); // 跳转到应用程序的复位处理程序
  }
  else
  {
    UART1_SendString("No valid application found!\r\n");
  }

}

/* 检查升级标志 */
uint8_t CheckUpgradeFlag(void) {
    uint32_t flag = *(__IO uint32_t*)UPGRADE_FLAG_ADDRESS;
    return (flag == UPGRADE_FLAG_VALUE);
}

/* 清除升级标志 */
void ClearUpgradeFlag(void) {
    HAL_FLASH_Unlock();
    //HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    HAL_FLASH_Lock();
}

// HAL_StatusTypeDef CopyFirmware(void)
// {
//   uint32_t src_addr = UPGRADE_START_ADDR;
//   uint32_t size = UPGRADE_SIZE;
  
//   uint32_t dest_addr = APP_ADDRESS;

//   FLASH_EraseInitTypeDef eraseInit = {0};
//   uint32_t sectorError = 0xFFFFFFFFU;
//   HAL_StatusTypeDef status = HAL_OK;

//   HAL_FLASH_Unlock();
//   // 配置擦除应用区
//   eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
//   eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
//   eraseInit.Sector = FLASH_SECTOR_2;
//   eraseInit.NbSectors = 6;  // 扇区2~7

//   // 一次性擦除
//   if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK)
//   {
//     UART1_SendString("Erase app area failed!\r\n");
//     status = HAL_ERROR;
//     goto cleanup;
//   }

//   // 按32位复制固件
//   for (uint32_t i = 0; i < size; i += 4U)
//   {
//     uint32_t data;
//     // 从升级区读取32位数据
//     memcpy(&data, (const void *)(src_addr + i), sizeof(data));
//     // 写入应用区
//     if (HAL_FLASH_Program(
//         FLASH_TYPEPROGRAM_WORD,
//         dest_addr + i,
//         data) != HAL_OK)
//     {
//       UART1_SendString("Write app area failed!\r\n");
//       status = HAL_ERROR;
//       goto cleanup;
//     }

//     // 每复制256字节翻转一次LED
//     if ((i % 256U) == 0U)
//     {
//       HAL_GPIO_TogglePin(GPIOF, LED_R_Pin);
//     }
//   }
//   cleanup:
//     // 无论成功失败，都重新锁定Flash
//     if (HAL_FLASH_Lock() != HAL_OK)
//     {
//         status = HAL_ERROR;
//     }

//     if (status == HAL_OK)
//     {
//         UART1_SendString("Firmware copy complete!\r\n");
//     }
//     return status;
// }

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

  uint8_t need_upgrade = 0;
  /* 按键检查是否需要升级 */
  if (KEY_PRESSED())
  {
    need_upgrade = 1;
  }

  if(need_upgrade)
  {
    //ClearUpgradeFlag();
    /* 进入IAP升级模式 */
    IAP_Init();
    /* 串口中断接收一个字节 */
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);

    uint32_t led_tick = HAL_GetTick();
    while(1){
      // 非阻塞LED闪烁
      if (HAL_GetTick() - led_tick >= 500) {
        led_tick = HAL_GetTick();
        HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
      }

      /* 主循环处理完整数据包 */
      if (rx_packet_ready)
      {
        IAP_Packet_t packet;
        /* 校验数据包是否正确 [注：验证数据CRC]*/
        if (IAP_ParsePacket(rx_buffer, rx_index, &packet)){
          /* 根据命令分发任务状态机 
           * [注：根据当前状态和接收到的事件，执行对应操作，并决定是否切换到下一个状态]
           * 有限状态机（FSM） 状态（State）、事件（Event）、状态转移（Transition）
          */
          IAP_ProcessPacket(&packet);
        }
        else{
          IAP_SendResponse(rx_buffer[1], RESP_CRC_ERROR);
        }
        rx_index = 0;
        rx_packet_ready = 0;
      }

      if(iap_info.state == IAP_STATE_COMPLETE)
      {
        HAL_Delay(1000);
        JumpToApplication(APP_ADDRESS);
      }
    }
  }
  HAL_Delay(2000);
  UART1_SendString("Checking application...\r\n");

  uint32_t app_sp = *(__IO uint32_t*)APP_ADDRESS;
  uint32_t app_entry = *(volatile uint32_t *)(APP_ADDRESS + 4U);
  // char buffer[64];
  // snprintf(buffer, sizeof(buffer),
  //        "App SP: 0x%08lX\r\n", (unsigned long)app_sp);
  // UART1_SendString(buffer);
  // snprintf(buffer, sizeof(buffer),
  //        "App Reset: 0x%08lX\r\n", (unsigned long)app_reset);
  // UART1_SendString(buffer);

  uint32_t sram_start = 0x20000000U;
  uint32_t sram_end = 0x20020000U;
  if ((app_sp >= sram_start) && (app_sp <= sram_end) && ((app_sp & 0x7U) == 0U) && ((app_entry & 1U) != 0U) && ((app_entry & ~1U) >= APP_ADDRESS) && ((app_entry & ~1U) < (APP_ADDRESS + APP_SIZE))){
    UART1_SendString("Valid application found\r\n");
    HAL_Delay(500);
    JumpToApplication(APP_ADDRESS);
  } 
  else {
    UART1_SendString("No valid application, waiting for upgrade\r\n");
    /* 进入IAP升级模式 */
    IAP_Init();
    while (1) {
      HAL_GPIO_TogglePin(GPIOF, LED_R_Pin);
      HAL_Delay(500);
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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        UART_RxCallback(rx_data);

        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}
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
