#include "flash.h"
#include "main.h"
#include <stm32f407xx.h>

// 解锁Flash
HAL_StatusTypeDef Flash_Unlock(void)
{
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    // FLASH->CR：读取 Flash 控制寄存器的值
    // FLASH_CR_LOCK：Flash 控制寄存器中的锁定位
    return (FLASH->CR & FLASH_CR_LOCK) ? HAL_ERROR : status;
}

// 锁定Flash
HAL_StatusTypeDef Flash_Lock(void)
{
    HAL_StatusTypeDef status = HAL_FLASH_Lock();
    return (FLASH->CR & FLASH_CR_LOCK) ? status : HAL_ERROR;
}