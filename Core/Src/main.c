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
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "bsp_esp8266.h"
#include "bsp_firmware.h"
#include "bsp_flash.h"
#include "bsp_iap.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
//#define KEY_PRESSED() HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 串口3 DMA接收缓冲区 */
#define ESP_RX_SIZE 256
uint8_t esp_rx_buffer[ESP_RX_SIZE];
uint8_t rx_data;
uint8_t need_upgrade = 0;

int active_bank;
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
  uint32_t app_end = 0U;
  if ((appAddress >= BANK_A_ADDRESS) &&
      (appAddress <= BANK_B_ADDRESS)) {
    app_end = appAddress + BANK_SIZE;
  }

  if ((app_end != 0U) &&
      /* The initial MSP may legally point one past the last SRAM byte. */
      (app_sp >= sram_start) && (app_sp <= sram_end) &&
      ((app_sp & 0x7U) == 0U) &&
      ((app_entry & 1U) != 0U) &&
      ((app_entry & ~1U) >= appAddress) &&
      ((app_entry & ~1U) < app_end))
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
  // (__IO uint32_t *)把整数地址转换为指向 32 位数据的 volatile 指针
  return (*(__IO uint32_t *)UPGRADE_FLAG_ADDRESS == UPGRADE_FLAG_VALUE) ? 1U : 0U;
}

/* 清除升级标志 */
void ClearUpgradeFlag(void) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0U;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return;
    }

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = UPGRADE_FLAG_SECTOR;
    erase_init.NbSectors = 1U;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    (void)HAL_FLASHEx_Erase(&erase_init, &sector_error);
    HAL_FLASH_Lock();
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();

  /* USER CODE BEGIN 2 */
  //HAL_UARTEx_ReceiveToIdle_DMA(&huart3,esp_rx_buffer,ESP_RX_SIZE);
  HAL_Delay(2000);

  //ESP8266_Init();
  
  // 待尝试启动，检查烧录的新Bank是否需要回滚
  if (CheckRollbackNeeded()) {
    UART1_SendString("Rollback needed, switching banks...\r\n");
    // 执行回滚操作
    if (PerformRollback() == 0) {
      HAL_Delay(1000);
      // 重新启动系统 
      NVIC_SystemReset();
    } 
    else {
      UART1_SendString("Rollback failed, entering recovery mode\r\n");
      // 进入恢复模式
      while (1) {
        HAL_Delay(200);
      }
    }
  }
  
  // APP请求升级 
  if (CheckUpgradeFlag()) {
    // 清除升级标志，防止重复升级
    ClearUpgradeFlag();
    // 进入 IAP 升级模式
    IAP_Init();
    // 启动串口接收中断
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);

    uint32_t led_tick = HAL_GetTick();
    while (1) {
      if (HAL_GetTick() - led_tick >= 500U) {
        led_tick = HAL_GetTick();
        HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
      }
      // 处理接收到的数据包
      if (rx_packet_ready) {
        IAP_Packet_t packet;
        // 解析接收到的数据包
        if (IAP_ParsePacket(rx_buffer, rx_index, &packet)) {
          // 处理数据包
          IAP_ProcessPacket(&packet);
        } else {
          IAP_SendResponse(rx_buffer[1], RESP_CRC_ERROR);
        }
        // 重置接收缓冲区索引和数据包就绪标志
        rx_index = 0U;
        rx_packet_ready = 0U;
      }
    }
  }

  active_bank = GetActiveBank();
  if (active_bank < 0) {
    UART1_SendString("No valid active bank, entering recovery mode\r\n");
    IAP_Init();
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    while (1) {
      if (rx_packet_ready) {
        IAP_Packet_t packet;
        if (IAP_ParsePacket(rx_buffer, rx_index, &packet)) {
          IAP_ProcessPacket(&packet);
        } else {
          IAP_SendResponse(rx_buffer[1], RESP_CRC_ERROR);
        }
        rx_index = 0U;
        rx_packet_ready = 0U;
      }
    }
  }
  
  // 记录启动尝试并验证活动分区
  if (RecordBootAttempt((BankIndex_t)active_bank) != 0 ||
      VerifyBankFirmware((BankIndex_t)active_bank) != 0) {
    UART1_SendString("Active bank verification failed\r\n");
    RecordBootFailure((BankIndex_t)active_bank);

    if (PerformRollback() == 0) {
      NVIC_SystemReset();
    }

    UART1_SendString("Entering recovery mode\r\n");
    while (1) {
      HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
      HAL_Delay(200);
    }
  }
  // 显示分区信息
  BankInfo_t info;
  ReadBankInfo((BankIndex_t)active_bank, &info);
  // printf("Firmware version: %u.%u.%u\r\n",
  //        info.version_major, info.version_minor, info.version_build);
  // printf("Boot count: %u\r\n", info.boot_count);
  // printf("Fail count: %u\r\n", info.fail_count);

  // 延时后跳转
  // printf("Jumping to application in 2 seconds...\r\n");
  HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
  HAL_Delay(2000);
  HAL_GPIO_TogglePin(GPIOF, LED_G_Pin);
  // 跳转到活动分区
  //JumpToBank(active_bank);
  JumpToApplication((active_bank == BANK_A) ?
                    BANK_A_ADDRESS : BANK_B_ADDRESS);
  //printf("Active bank: %c\r\n", 'A' + active_bank);
  //   HAL_Delay(2000);
  //   UART1_SendString("Checking application...\r\n");

  //   uint32_t app_sp = *(__IO uint32_t*)APP_ADDRESS;
  //   uint32_t app_entry = *(volatile uint32_t *)(APP_ADDRESS + 4U);
  //   // char buffer[64];
  //   // snprintf(buffer, sizeof(buffer),
  //   //        "App SP: 0x%08lX\r\n", (unsigned long)app_sp);
  //   // UART1_SendString(buffer);
  //   // snprintf(buffer, sizeof(buffer),
  //   //        "App Reset: 0x%08lX\r\n", (unsigned long)app_reset);
  //   // UART1_SendString(buffer);

  //   uint32_t sram_start = 0x20000000U;
  //   uint32_t sram_end = 0x20020000U;
  //   if ((app_sp >= sram_start) && (app_sp <= sram_end) && ((app_sp & 0x7U) == 0U) && ((app_entry & 1U) != 0U) && ((app_entry & ~1U) >= APP_ADDRESS) && ((app_entry & ~1U) < (APP_ADDRESS + APP_SIZE))){
  //     UART1_SendString("Valid application found\r\n");
  //     HAL_Delay(500);
  //     JumpToApplication(APP_ADDRESS);
  //   } 
  //   else {
  //     UART1_SendString("No valid application, waiting for upgrade\r\n");
  //     /* 进入IAP升级模式 */
  //     IAP_Init();
  //     while (1) {
  //       HAL_GPIO_TogglePin(GPIOF, LED_R_Pin);
  //       HAL_Delay(500);
  //     }
  //   }
  // }
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART3)
  {
    /* [注：Size是HAL库计算出的接收的数据大小] */
    HAL_UART_Transmit(&huart1,esp_rx_buffer,Size,1000);
    /*重新启动USART3接收 [注：ESP_RX_SIZE是最大允许接收的大小] */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3,esp_rx_buffer,ESP_RX_SIZE);
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
