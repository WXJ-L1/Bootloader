#ifndef __BSP_FLASH_H__
#define __BSP_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/*---------------- Bootloader ----------------*/
#define BOOTLOADER_START_ADDR       0x08000000UL
#define BOOTLOADER_END_ADDR         0x0800FFFFUL
#define BOOTLOADER_SIZE             0x00010000UL      /* 64KB */

/*---------------- Bank metadata ----------------*/
#define BANK_INFO_START_ADDR        0x08010000UL
#define BANK_INFO_END_ADDR          0x0801FFFFUL
#define BANK_INFO_SIZE              0x00010000UL      /* 64KB */

#define BANK_A_INFO_ADDRESS         0x08010000UL
#define BANK_B_INFO_ADDRESS         0x08011000UL

/*---------------- Bank A ----------------*/
#define BANK_A_START_ADDR           0x08020000UL
#define BANK_A_END_ADDR             0x0807FFFFUL
#define BANK_A_SIZE                 0x00060000UL      /* 384KB */

#define BANK_A_ADDRESS              BANK_A_START_ADDR
#define BANK_A_SECTOR_COUNT         3U

/*---------------- Bank B ----------------*/
#define BANK_B_START_ADDR           0x08080000UL
#define BANK_B_END_ADDR             0x080DFFFFUL
#define BANK_B_SIZE                 0x00060000UL      /* 384KB */

#define BANK_B_ADDRESS              BANK_B_START_ADDR
#define BANK_B_SECTOR_COUNT         3U

/*---------------- Recovery / reserved area ----------------*/
#define RECOVERY_START_ADDR         0x080E0000UL
#define RECOVERY_END_ADDR           0x080FFFFFUL
#define RECOVERY_SIZE               0x00020000UL      /* 128KB */

#define UPGRADE_FLAG_ADDRESS  0x080E0000UL
#define UPGRADE_FLAG_VALUE    0x55504752UL
#define UPGRADE_FLAG_SECTOR   FLASH_SECTOR_11

/*---------------- Common definitions ----------------*/
#define BANK_COUNT                  2U
#define BANK_SIZE                   BANK_A_SIZE

#define FLASH_START_ADDR            0x08000000UL
#define FLASH_END_ADDR              0x080FFFFFUL
#define FLASH_TOTAL_SIZE            0x00100000UL      /* 1MB */

// 分区状态枚举
typedef enum {
    BANK_STATE_EMPTY = 0,        // 空分区（未写入固件）
    BANK_STATE_UPDATING,         // 正在更新
    BANK_STATE_READY,            // 就绪（已验证，可启动）
    BANK_STATE_ACTIVE,           // 活动（当前运行）
    BANK_STATE_INVALID,          // 无效（验证失败）
    BANK_STATE_ROLLBACK,         // 回滚（从此分区回滚）
    BANK_STATE_PENDING           // 待尝试启动，APP确认后再转ACTIVE
} BankState_t;

// 分区信息结构
typedef struct {
    uint32_t magic;              // 魔数：0x424E4B49 ("BNKI")
    BankState_t state;           // 分区状态
    uint32_t version_major;      // 主版本号
    uint32_t version_minor;      // 次版本号
    uint32_t version_build;      // 编译号
    uint32_t firmware_size;      // 固件大小
    uint32_t firmware_crc32;     // 固件CRC32
    uint32_t update_timestamp;   // 更新时间戳
    uint32_t boot_count;         // 启动次数
    uint32_t fail_count;         // 失败次数
    uint32_t reserved[6];        // 保留字段
} BankInfo_t;

// 分区索引
typedef enum {
    BANK_A = 0,
    BANK_B = 1
} BankIndex_t;

// 最大启动失败次数
#define MAX_BOOT_FAILURES 3

int GetActiveBank(void);

int ReadBankInfo(BankIndex_t bank, BankInfo_t *info);

int VerifyBankFirmware(BankIndex_t bank);
int SwitchToBank(BankIndex_t target_bank);
int UpdateBankState(BankIndex_t bank, BankState_t new_state);
int RecordBootAttempt(BankIndex_t bank);
int RecordBootFailure(BankIndex_t bank);
int PerformRollback(void);
int CheckRollbackNeeded(void);

int DualBank_StartUpgrade(uint32_t firmware_size, uint32_t firmware_crc32,
                          uint32_t version_major, uint32_t version_minor,
                          uint32_t version_build);
int DualBank_WriteFirmware(uint32_t offset, const uint8_t *data, uint32_t length);
int DualBank_FinishUpgrade(void);

#ifdef __cplusplus
}
#endif
#endif /*__BSP_FLASH_H__ */
