#ifndef __FIRMWARE_H__
#define __FIRMWARE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* IAP命令定义 */
#define CMD_START_UPGRADE    0x01  // 开始升级
#define CMD_SEND_DATA        0x02  // 发送数据
#define CMD_END_UPGRADE      0x03  // 结束升级
#define CMD_GET_VERSION      0x04  // 获取版本
#define CMD_ERASE_FLASH      0x05  // 擦除Flash
#define CMD_JUMP_TO_APP      0x06  // 跳转到应用

/* 响应码定义 */
#define RESP_OK              0x00  // 成功
#define RESP_ERROR           0x01  // 错误
#define RESP_CRC_ERROR       0x02  // CRC错误
#define RESP_TIMEOUT         0x03  // 超时
#define RESP_FLASH_ERROR     0x04  // Flash操作错误

/* 数据包定义 */
#define PACKET_SOF           0xAA
#define PACKET_EOF           0x55
#define MAX_DATA_SIZE        1024

typedef struct {
    uint8_t sof;                    // SOF: 帧起始标志，固定为0xAA
    uint8_t cmd;                    // CMD: 命令字
    uint16_t len;                   // LEN: 数据长度
    uint8_t data[MAX_DATA_SIZE];    // DATA:有效数据
    uint16_t crc;                   // CRC: CRC16校验和
    uint8_t eof;                    // EOF: 帧结束标志，固定为0x55
} IAP_Packet_t;

extern uint8_t rx_buffer[MAX_DATA_SIZE + 10];
extern uint16_t rx_index;

void IAP_SendResponse(uint8_t cmd, uint8_t status);
uint16_t CRC16_Calculate(uint8_t *data, uint16_t length);
uint8_t IAP_ParsePacket(uint8_t *buffer, uint16_t length, IAP_Packet_t *packet);
void UART_RxCallback(uint8_t data);
#ifdef __cplusplus
}
#endif
#endif /*__ FIRMWARE_H__ */