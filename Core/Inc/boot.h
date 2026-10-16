#ifndef __BOOT_H__
#define __BOOT_H__

#ifdef __cplusplus
extern "C" {
#endif

// 固件下载协议定义
#define CMD_START       0xA5    // 开始下载命令
#define CMD_DATA        0xA6    // 数据包命令
#define CMD_END         0xA7    // 结束下载命令
#define CMD_ERASE       0xA8    // 擦除Flash命令

#define ACK_OK          0x79    // 应答成功
#define ACK_ERROR       0x1F    // 应答失败

/* 数据包结构 */
typedef struct{
    uint8_t cmd;
    uint16_t seq;
    uint16_t length;
    uint8_t checksum;   // 校验和
    uint8_t data[256];
} FirmwarePacket;

void FirmwareDownload(void);

#ifdef __cplusplus
}
#endif
#endif /*__ BOOT_H__ */
