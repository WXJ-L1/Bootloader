#ifndef __IAP_H__
#define __IAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "bsp_firmware.h"

/* IAP 状态定义 */
typedef enum {
    IAP_STATE_IDLE = 0,      // 空闲状态
    IAP_STATE_READY,         // 准备升级
    IAP_STATE_RECEIVING,     // 接收数据
    IAP_STATE_VERIFYING,     // 校验固件
    IAP_STATE_COMPLETE,      // 升级完成
    IAP_STATE_ERROR          // 升级错误
} IAP_State_t;

/* IAP 信息结构 */
typedef struct {
    IAP_State_t state;           // 当前状态
    uint32_t app_address;        // 应用程序地址
    uint32_t app_size;           // 应用程序大小
    uint32_t received_size;      // 已接收大小
    uint32_t total_size;         // 总大小
    uint32_t crc32;              // 固件CRC32
    uint32_t write_address;      // 当前写入地址
} IAP_Info_t;

extern IAP_Info_t iap_info;
extern volatile uint8_t rx_packet_ready;

void IAP_Init(void);
void IAP_ProcessPacket(IAP_Packet_t *packet);

#ifdef __cplusplus
}
#endif
#endif /*__ IAP_H__ */