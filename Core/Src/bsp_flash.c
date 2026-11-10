#include "main.h"

#include <stm32f407xx.h>
#include <string.h>

#include "bsp_flash.h"
#include "bsp_iap.h"

/**
 * @brief 读取分区信息
 * @param bank 分区索引
 * @param info 输出的分区信息
 * @return 0成功，-1失败
 */
int ReadBankInfo(BankIndex_t bank, BankInfo_t *info) {
    uint32_t info_address = BANK_INFO_START_ADDR + bank * sizeof(BankInfo_t);
    /* 从Flash读取 */
    memcpy(info, (void*)info_address, sizeof(BankInfo_t));
    /* 验证魔数 */
    if (info->magic != 0x424E4B49) {
        //printf("Invalid bank info magic for Bank %c\r\n", 'A' + bank);
        return -1;
    }
    return 0;
}

/**
 * @brief 写入分区信息
 * @param bank 分区索引
 * @param info 分区信息
 * @return 0成功，-1失败
 */
int WriteBankInfo(BankIndex_t bank, BankInfo_t *info)
{
    if (info == NULL) {
        return -1;
    }
    if (bank != BANK_A && bank != BANK_B) {
        return -1;
    }

    uint32_t bank_info_address = BANK_INFO_START_ADDR + ((uint32_t)bank * sizeof(BankInfo_t));
    uint32_t other_bank = (bank == BANK_A) ? BANK_B : BANK_A;
    uint32_t other_info_address = BANK_INFO_START_ADDR + (other_bank * sizeof(BankInfo_t));

    BankInfo_t other_info;
    memcpy(&other_info, (const void *)other_info_address, sizeof(BankInfo_t));

    BankInfo_t write_info = *info;
    write_info.magic = 0x424E4B49UL;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }

    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_EOP      |
        FLASH_FLAG_OPERR    |
        FLASH_FLAG_WRPERR   |
        FLASH_FLAG_PGAERR   |
        FLASH_FLAG_PGPERR   |
        FLASH_FLAG_PGSERR
    );

    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_4;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }

    const uint32_t *other_data = (const uint32_t *)&other_info;
    uint32_t word_count = sizeof(BankInfo_t) / sizeof(uint32_t);
    for (uint32_t i = 0; i < word_count; i++) {
        if (HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_WORD,
                other_info_address + i * 4U,
                other_data[i]) != HAL_OK) {

            HAL_FLASH_Lock();
            return -1;
        }
    }

    const uint32_t *write_data = (const uint32_t *)&write_info;
    for (uint32_t i = 0; i < word_count; i++) {
        if (HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_WORD,
                bank_info_address + i * 4U,
                write_data[i]) != HAL_OK) {

            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    if (memcmp((const void *)bank_info_address, &write_info, sizeof(BankInfo_t)) != 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief 更新分区状态
 * @param bank 分区索引
 * @param new_state 新状态
 * @return 0成功，-1失败
 */
int UpdateBankState(BankIndex_t bank, BankState_t new_state) {
    BankInfo_t info;
    // 读取当前信息
    if (ReadBankInfo(bank, &info) != 0) {
        // 如果读取失败，初始化新信息
        memset(&info, 0, sizeof(BankInfo_t));
    }
    // 更新状态
    info.state = new_state;
    // 写回
    return WriteBankInfo(bank, &info);
}

/**
 * @brief 获取当前活动分区
 * @return BANK_A或BANK_B，-1表示无活动分区
 */
int GetActiveBank(void) {
    BankInfo_t info_a, info_b;

    // 读取两个分区的信息
    int ret_a = ReadBankInfo(BANK_A, &info_a);
    int ret_b = ReadBankInfo(BANK_B, &info_b);

    /* PENDING优先于ACTIVE：升级后下次启动必须先尝试新Bank。 */
    if (ret_a == 0 && info_a.state == BANK_STATE_PENDING) {
        return BANK_A;
    }

    if (ret_b == 0 && info_b.state == BANK_STATE_PENDING) {
        return BANK_B;
    }

    if (ret_a == 0 && info_a.state == BANK_STATE_ACTIVE) {
        return BANK_A;
    }

    if (ret_b == 0 && info_b.state == BANK_STATE_ACTIVE) {
        return BANK_B;
    }

    // 如果都不是活动状态，选择版本较新的就绪分区
    if (ret_a == 0 && info_a.state == BANK_STATE_READY) {
        if (ret_b == 0 && info_b.state == BANK_STATE_READY) {
            // 比较版本号
            if (info_a.version_major > info_b.version_major ||
                (info_a.version_major == info_b.version_major && 
                 info_a.version_minor > info_b.version_minor) ||
                (info_a.version_major == info_b.version_major && 
                 info_a.version_minor == info_b.version_minor &&
                 info_a.version_build > info_b.version_build)) {
                return BANK_A;
            } else {
                return BANK_B;
            }
        }
        return BANK_A;
    }

    if (ret_b == 0 && info_b.state == BANK_STATE_READY) {
        return BANK_B;
    }
    //printf("No active bank found\r\n");
    return -1;
}

/**
 * @brief 获取非活动分区（用于升级）
 * @return BANK_A或BANK_B，-1表示错误
 */
int GetInactiveBank(void) {
    int active_bank = GetActiveBank();
    if (active_bank == BANK_A) {
        return BANK_B;
    } else if (active_bank == BANK_B) {
        return BANK_A;
    }
    // 如果没有活动分区，默认使用Bank A
    return BANK_A;
}

/**
 * @brief 验证分区固件
 * @param bank 分区索引
 * @return 0验证成功，-1验证失败
 */
int VerifyBankFirmware(BankIndex_t bank) {
    BankInfo_t info;
    // 读取分区信息
    if (ReadBankInfo(bank, &info) != 0) {
        //printf("Failed to read bank info\r\n");
        return -1;
    }
    // 检查固件大小
    if (info.firmware_size == 0 || info.firmware_size > BANK_SIZE) {
        //printf("Invalid firmware size: %u\r\n", info.firmware_size);
        return -1;
    }
    // 获取分区地址
    uint32_t bank_address = (bank == BANK_A) ? BANK_A_ADDRESS : BANK_B_ADDRESS;
    //printf("Verifying Bank %c firmware...\r\n", 'A' + bank);
    //printf("  Address: 0x%08X\r\n", bank_address);
    //printf("  Size: %u bytes\r\n", info.firmware_size);
    // 计算CRC32
    uint32_t calculated_crc = CRC32_Calculate(
        (const uint8_t *)bank_address,
        info.firmware_size
    );
    //printf("  Expected CRC32: 0x%08X\r\n", info.firmware_crc32);
    //printf("  Calculated CRC32: 0x%08X\r\n", calculated_crc);
    // 验证CRC
    if (calculated_crc != info.firmware_crc32) {
        //printf("CRC verification failed!\r\n");
        return -1;
    }
    // 验证固件头部（检查栈指针是否有效）
    uint32_t stack_pointer = *(__IO uint32_t*)bank_address;
    /* The vector table stores the initial MSP, which may equal SRAM end. */
    if ((stack_pointer < SRAM_BASE) ||
        (stack_pointer > (SRAM_BASE + (128U * 1024U))) ||
        ((stack_pointer & 0x7U) != 0U)) {
        //printf("Invalid stack pointer: 0x%08X\r\n", stack_pointer);
        return -1;
    }
    //printf("Bank %c firmware verified successfully\r\n", 'A' + bank);
    return 0;
}

/**
 * @brief 切换到指定分区
 * @param target_bank 目标分区
 * @return 0成功，-1失败
 */
int SwitchToBank(BankIndex_t target_bank) {
    BankInfo_t info;
    //printf("Switching to Bank %c...\r\n", 'A' + target_bank);
    // 验证目标分区，包括固件长度、CRC、Hash或向量表等。验证失败就不切换
    if (VerifyBankFirmware(target_bank) != 0) {
        //printf("Target bank verification failed\r\n");
        return -1;
    }
    // 读取目标分区信息
    if (ReadBankInfo(target_bank, &info) != 0) {
        return -1;
    }
    // 获取当前活动分区
    int current_bank = GetActiveBank();
    // 如果有当前活动分区，将其状态改为READY
    if (current_bank >= 0) {
        UpdateBankState(current_bank, BANK_STATE_READY);
        //printf("Bank %c set to READY\r\n", 'A' + current_bank);
    }
    // 将目标分区状态改为ACTIVE，并清零启动次数和失败次数
    info.state = BANK_STATE_ACTIVE;
    info.boot_count = 0;
    info.fail_count = 0;
    if (WriteBankInfo(target_bank, &info) != 0) {
        //printf("Failed to update target bank state\r\n");
        return -1;
    }
    //printf("Bank %c set to ACTIVE\r\n", 'A' + target_bank);
    //printf("Switch complete, system will reset\r\n");
    return 0;
}

/**
 * @brief 记录启动尝试
 * @param bank 分区索引
 * @return 0成功，-1失败
 */
int RecordBootAttempt(BankIndex_t bank) {
    BankInfo_t info;

    if (ReadBankInfo(bank, &info) != 0) {
        return -1;
    }
    info.boot_count++;
    //printf("Bank %c boot attempt: %u\r\n", 'A' + bank, info.boot_count);
    return WriteBankInfo(bank, &info);
}

/**
 * @brief 记录启动失败
 * @param bank 分区索引
 * @return 0成功，-1失败
 */
int RecordBootFailure(BankIndex_t bank) {
    BankInfo_t info;

    if (ReadBankInfo(bank, &info) != 0) {
        return -1;
    }
    info.fail_count++;
    //printf("Bank %c boot failure: %u\r\n", 'A' + bank, info.fail_count);
    // 检查是否超过最大失败次数
    if (info.fail_count >= MAX_BOOT_FAILURES) {
        //printf("Bank %c exceeded max failures, marking as invalid\r\n", 'A' + bank);
        info.state = BANK_STATE_INVALID;
    }
    return WriteBankInfo(bank, &info);
}

/**
 * @brief 执行回滚
 * @return 0成功，-1失败
 */
int PerformRollback(void) {
    // 获取当前准备启动或正在使用的活动分区
    int active_bank = GetActiveBank();
    if (active_bank < 0) {
        //printf("No active bank to rollback from\r\n");
        return -1;
    }
    //printf("Performing rollback from Bank %c\r\n", 'A' + active_bank);
    // 把当前分区标记为回滚状态，表示当前分区启动失败或不再可信
    UpdateBankState(active_bank, BANK_STATE_ROLLBACK);
    // 获取另一个分区
    int rollback_bank = (active_bank == BANK_A) ? BANK_B : BANK_A;
    // 验证回滚目标
    if (VerifyBankFirmware(rollback_bank) != 0) {
        //printf("Rollback target Bank %c is invalid\r\n", 'A' + rollback_bank);
        return -1;
    }
    // 切换到回滚目标
    return SwitchToBank(rollback_bank);
}

/**
 * @brief 检查是否需要回滚
 * @return 1需要回滚，0不需要
 */
int CheckRollbackNeeded(void) {
    // 获取当前准备启动或正在使用的活动分区
    int active_bank = GetActiveBank();
    if (active_bank < 0) {
        return 0;
    }
    BankInfo_t info;
    if (ReadBankInfo(active_bank, &info) != 0) {
        return 0;
    }
    // 只有未确认的新Bank启动失败达到上限才需要回滚
    if (info.state == BANK_STATE_PENDING &&
        info.boot_count >= MAX_BOOT_FAILURES) {
        // printf("Bank %c has too many failures, rollback needed\r\n", 'A' + active_bank);
        return 1;
    }
    return 0;
}

/**
 * @brief 开始双区升级
 * @param firmware_size 固件大小
 * @param firmware_crc32 固件CRC32
 * @param version_major 主版本号
 * @param version_minor 次版本号
 * @param version_build 编译号
 * @return 0成功，-1失败
 */
int DualBank_StartUpgrade(uint32_t firmware_size, uint32_t firmware_crc32,
                          uint32_t version_major, uint32_t version_minor,
                          uint32_t version_build) {
    // 获取非活动分区
    int target_bank = GetInactiveBank();
    if (target_bank < 0) {
        return -1;
    }
    // 准备分区信息
    BankInfo_t info;
    memset(&info, 0, sizeof(BankInfo_t));
    info.state = BANK_STATE_UPDATING;
    info.version_major = version_major;
    info.version_minor = version_minor;
    info.version_build = version_build;
    info.firmware_size = firmware_size;
    info.firmware_crc32 = firmware_crc32;
    info.update_timestamp = HAL_GetTick();
    // 写入分区信息
    if (WriteBankInfo(target_bank, &info) != 0) {
        //printf("Failed to write bank info\r\n");
        return -1;
    }
    // 擦除目标分区
    uint32_t bank_address = (target_bank == BANK_A) ? BANK_A_ADDRESS : BANK_B_ADDRESS;
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }

    uint32_t address = bank_address;
    uint32_t end_address = bank_address + BANK_SIZE;
    /* 定义Flash擦除结构体 */
    while (address < end_address) {
        uint32_t sector = FLASH_GetSector(address);
        if (sector == 0xFFFFFFFFUL) {
            HAL_FLASH_Lock();
            return -1;
        }
        FLASH_EraseInitTypeDef erase_init;
        uint32_t sector_error = 0U;
        erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase_init.Sector = sector;
        erase_init.NbSectors = 1U;
        erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(
                &erase_init,
                &sector_error) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }

        if (sector <= FLASH_SECTOR_3) {
            address += 0x4000UL;       /* 16KB */
        } else if (sector == FLASH_SECTOR_4) {
            address += 0x10000UL;      /* 64KB */
        } else {
            address += 0x20000UL;      /* 128KB */
        }
    }
    HAL_FLASH_Lock();

    iap_info.state = IAP_STATE_READY;
    iap_info.target_bank = target_bank;
    iap_info.target_address = (target_bank == BANK_A) ? BANK_A_ADDRESS : BANK_B_ADDRESS;
    iap_info.write_address = iap_info.target_address;
    iap_info.total_size = firmware_size;
    iap_info.crc32 = firmware_crc32;
    iap_info.received_size = 0;

    return 0;
}

/**
 * @brief 写入固件数据
 * @param offset 偏移量
 * @param data 数据
 * @param length 数据长度
 * @return 0成功，-1失败
 */
int DualBank_WriteFirmware(uint32_t offset, const uint8_t *data, uint32_t length){
    if (data == NULL || length == 0U) {
        return -1;
    }

    if (iap_info.target_bank != BANK_A &&
        iap_info.target_bank != BANK_B) {
        return -1;
    }

    if (offset > BANK_SIZE || length > BANK_SIZE - offset) {
        return -1;
    }

    uint32_t bank_address = (iap_info.target_bank == BANK_A) ? BANK_A_ADDRESS : BANK_B_ADDRESS;
    uint32_t write_address = bank_address + offset;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }

    uint32_t bytes_written = 0U;

    while (bytes_written < length) {
        uint32_t word = 0xFFFFFFFFUL;
        uint32_t remaining = length - bytes_written;
        uint32_t copy_len = (remaining >= 4U) ? 4U : remaining;
        memcpy(&word, &data[bytes_written], copy_len);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
        write_address += 4U;
        bytes_written += copy_len;
    }
    HAL_FLASH_Lock();
    return 0;
}

/**
 * @brief 完成升级
 * @return 0成功，-1失败
 */
int DualBank_FinishUpgrade(void)
{
    int target_bank = iap_info.target_bank;

    if (target_bank != BANK_A &&
        target_bank != BANK_B) {
        return -1;
    }

    if (iap_info.received_size != iap_info.total_size) {
        return -1;
    }

    uint32_t calculated_crc =
        CRC32_Calculate(
            (const uint8_t *)iap_info.target_address,
            iap_info.total_size
        );

    if (calculated_crc != iap_info.crc32) {
        UpdateBankState(
            target_bank,
            BANK_STATE_INVALID
        );
        return -1;
    }

    if (VerifyBankFirmware(target_bank) != 0) {
        UpdateBankState(
            target_bank,
            BANK_STATE_INVALID
        );
        return -1;
    }

    if (UpdateBankState(
            target_bank,
            BANK_STATE_READY) != 0) {
        return -1;
    }

    return 0;
}
