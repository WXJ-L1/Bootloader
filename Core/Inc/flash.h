#ifndef __FLASH_H__
#define __FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
// Flash地址定义
#define BOOTLOADER_START_ADDR   0x08000000
#define BOOTLOADER_SIZE         0x00008000  // 32KB

#define APP_ADDRESS             0x08008000
#define APP_SIZE                0x00078000  // 480KB

#define UPGRADE_START_ADDR      0x08080000
#define UPGRADE_SIZE            0x00060000  // 384KB (Sector 8~10)

#define UPGRADE_FLAG_ADDRESS    0x080E0000  // Sector 11 起始地址
#define UPGRADE_FLAG_SIZE       0x00020000  // 128KB
#define UPGRADE_FLAG_VALUE      0xA5A5A5A5  // 升级标志值

// 函数声明
void Flash_Unlock(void);
void Flash_Lock(void);

#ifdef __cplusplus
}
#endif
#endif /*__ FLASH_H__ */
