
#include "stdint.h"
#include "boot.h"
#include "flash.h"
#include <usart.h>
#include <string.h>
#include <stm32f4xx_hal_flash_ex.h>

// 计算校验和
uint8_t CalculateChecksum(uint8_t *data, uint16_t length)
{
    uint8_t sum = 0;
    for(uint16_t i = 0; i < length; i++)
    {
        sum += data[i];
    }
    return sum;
}

void FirmwareDownload(void)
{
    FirmwarePacket packet;

    uint32_t wire_addr = UPGRADE_START_ADDR;
    uint16_t expected_seq = 0;
    uint8_t cmd;
    /* FLASH 擦除参数，连续擦除8、9、10扇区
     * STM32F407的1MB Flash分为12个扇区，大小如下：
        * 扇区0: 16KB (0x08000000 - 0x08003FFF)
        * 扇区1: 16KB (0x08004000 - 0x08007FFF)
        * 
        * 扇区2: 16KB (0x08008000 - 0x0800BFFF)
        * 扇区3: 16KB (0x0800C000 - 0x0800FFFF)
        * 扇区4: 64KB (0x08010000 - 0x0801FFFF)
        * 扇区5: 128KB (0x08020000 - 0x0803FFFF)
        * 扇区6: 128KB (0x08040000 - 0x0805FFFF)
        * 扇区7: 128KB (0x08060000 - 0x0807FFFF)
        * 
        * 扇区8: 128KB (0x08080000 - 0x0809FFFF)
        * 扇区9: 128KB (0x080A0000 - 0x080BFFFF)
        * 扇区1₀: 128KB (0x080C0000 - 0x080DFFFF)
        * 
        * 扇区1₁: 128KB (0x080E0000 - 0x080FFFFF)
        * 4×16 + 64 + 7×128 = 1024KB（1MB）
        */
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError = 0xFFFFFFFFU;
    /* 扇区擦除模式 */
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    /* 电压范围3，适用于2.7V~3.6V */
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    /* 擦除扇区8、9、10 */
    eraseInit.Sector = FLASH_SECTOR_8;
    eraseInit.NbSectors = 3;

    UART1_SendString("\r\nBootloader Ready. Waiting for firmware...\r\n");
    while(1){
        // 接收
        cmd = UART1_ReceiveByte(5000);
        switch (cmd){
            case CMD_START:
                UART1_SendString("Firmware download started.\r\n");
                expected_seq = 0;
                wire_addr = UPGRADE_START_ADDR;
                UART1_SendByte(ACK_OK);
                break;

            case CMD_ERASE:
                UART1_SendString("Erasing flash...\r\n");
                // 擦除升级区域的所有扇区
                HAL_FLASH_Unlock();
                if(HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK){
                    UART1_SendString("Flash erase failed!\r\n");
                    UART1_SendByte(ACK_ERROR);
                    HAL_FLASH_Lock();
                    return;
                }
                HAL_FLASH_Lock();

                UART1_SendString("Flash erase completed.\r\n");
                UART1_SendByte(ACK_OK);
                break;
            
            case CMD_DATA:
                // 接收序号（2字节）
                packet.seq = UART1_ReceiveByte(1000);
                packet.seq |= (UART1_ReceiveByte(1000) << 8);

                packet.length = UART1_ReceiveByte(1000);
                packet.length |= (UART1_ReceiveByte(1000) << 8);

                if(packet.seq != expected_seq){
                    UART1_SendString("Sequence number mismatch!\r\n");
                    UART1_SendByte(ACK_ERROR);
                    continue;
                }
                
                if(UART1_ReceiveBuffer(packet.data, packet.length, 1000) != packet.length){
                    UART1_SendString("Data receive error!\r\n");
                    UART1_SendByte(ACK_ERROR);
                    continue;
                }

                // 接收校验和
                packet.checksum = UART1_ReceiveByte(1000);
                uint8_t calc_sum = CalculateChecksum(packet.data, packet.length);
                if(calc_sum != packet.checksum){
                    UART1_SendString("Checksum mismatch!\r\n");
                    UART1_SendByte(ACK_ERROR);
                    continue;
                }
                // 写入Flash
                HAL_FLASH_Unlock();
                 // 按32位写入 Flash
                HAL_StatusTypeDef status = HAL_OK;

                for (uint32_t i = 0; i < packet.length; i += 4)
                {
                    /* 将接收到的数据按32位对齐 */
                    uint32_t data = 0xFFFFFFFFU;
                    /* 计算剩余字节数，确保不会越界 */
                    uint32_t remaining = packet.length - i;
                    /* 计算要写入的字节数 */
                    uint32_t bytes = (remaining >= 4U) ? 4U : remaining;

                    memcpy(&data, &packet.data[i], bytes);
                    /* 连续写入32位数据 */
                    status = HAL_FLASH_Program(
                        FLASH_TYPEPROGRAM_WORD,
                        wire_addr + i,
                        data
                    );

                    if (status != HAL_OK)
                    {
                        UART1_SendByte(ACK_ERROR);
                        break;
                    }
                }
                HAL_FLASH_Lock();

                wire_addr += packet.length;
                expected_seq++;
                UART1_SendByte(ACK_OK);
                break;
            
            case CMD_END:
                UART1_SendString("Firmware download completed.\r\n");
                UART1_SendByte(ACK_OK);
                return;
            
            default:
                break;
        }
    }
}