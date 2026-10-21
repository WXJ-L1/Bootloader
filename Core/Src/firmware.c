#include "string.h"
#include <stdint.h>
#include "iap.h"
#include "firmware.h"
#include "usart.h"

/* 接收缓冲区 */
uint8_t rx_buffer[MAX_DATA_SIZE + 10];
uint16_t rx_index = 0;

extern UART_HandleTypeDef huart1;

/* CRC16计算 （看视频学习）*/ 
uint16_t CRC16_Calculate(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  发送IAP命令响应数据包
 * @details 根据命令码和状态码构建响应数据包，
 *          计算CRC16校验值，并通过USART1发送。
 * @param  cmd    IAP命令码
 * @param  status 命令执行结果状态码
 * @retval None
 */
void IAP_SendResponse(uint8_t cmd, uint8_t status) {
    uint8_t response[8];

    response[0] = PACKET_SOF;
    response[1] = cmd;
    response[2] = 0x01;  // 长度低字节
    response[3] = 0x00;  // 长度高字节
    response[4] = status;

    uint16_t crc = CRC16_Calculate(&response[1], 4);
    response[5] = crc & 0xFF;
    response[6] = (crc >> 8) & 0xFF;
    response[7] = PACKET_EOF;

    // 通过UART发送
    HAL_UART_Transmit(&huart1, response, sizeof(response), HAL_MAX_DELAY);
}

/**
 * @brief  解析并校验IAP通信数据包
 * @details 检查数据包长度、帧头、帧尾及CRC16校验值，
 *          校验通过后将数据解析并保存到IAP_Packet_t结构体中。
 * @param  buffer 指向接收数据缓冲区的指针
 * @param  length 接收到的数据包总长度（字节）
 * @param  packet 指向IAP数据包结构体的指针，用于保存解析结果
 * @retval 1 数据包解析及校验成功
 * @retval 0 数据包解析或校验失败
 */
uint8_t IAP_ParsePacket(uint8_t *buffer, uint16_t length, IAP_Packet_t *packet) {
    /* 检查最小长度[注：总长度7 + N] */
    if (length < 7) {
        return 0;
    }
    /* 检查帧头和帧尾 */ 
    if (buffer[0] != PACKET_SOF || buffer[length - 1] != PACKET_EOF) {
        return 0;
    }
    /* 解析字段保存到IAP_Packet_t结构体 */ 
    packet->sof = buffer[0];
    packet->cmd = buffer[1];
    packet->len = buffer[2] | (buffer[3] << 8);
    /* 检查数据长度 */ 
    if (packet->len > MAX_DATA_SIZE || length != (packet->len + 7)) {
        return 0;
    }
    /* 复制数据 */ 
    memcpy(packet->data, &buffer[4], packet->len);
    /* 提取CRC16值 */ 
    packet->crc = buffer[4 + packet->len] | (buffer[5 + packet->len] << 8);
    packet->eof = buffer[6 + packet->len];
    /* 验证CRC [注：计算CRC时不包括SOF和EOF] */ 
    uint16_t calculated_crc = CRC16_Calculate(&buffer[1], packet->len + 3);
    if (calculated_crc != packet->crc) {
        return 0; 
    }
    return 1;
}

/**
 * @brief  UART串口接收数据状态机
 * @details 逐字节接收串口数据，根据帧头、命令、数据长度等字段，
 *          将接收到的字节组装成完整的IAP数据包并保存到rx_buffer中。
 *          数据包接收完成后，设置rx_packet_ready标志，
 *          通知主循环进行数据包解析和命令处理。
 * @param  data 串口接收到的单个字节
 * @retval None
 */
void UART_RxCallback(uint8_t data) {
    if (rx_packet_ready) return;
    static uint8_t state = 0;
    static uint16_t data_len = 0;

    switch (state) {
        case 0:  // 等待SOF
            if (data == PACKET_SOF) {
                rx_buffer[0] = data;
                rx_index = 1;
                state = 1;
            }
            break;

        case 1:  // 接收CMD
            rx_buffer[rx_index++] = data;
            state = 2;
            break;

        case 2:  // 接收LEN低字节
            rx_buffer[rx_index++] = data;
            data_len = data;
            state = 3;
            break;

        case 3:  // 接收LEN高字节
            rx_buffer[rx_index++] = data;
            data_len |= (data << 8);
            state = 4;
            break;

        case 4:  // 接收数据和CRC、EOF
            rx_buffer[rx_index++] = data;
            if (rx_index >= (data_len + 7)) {
                rx_packet_ready = 1;
                // 数据包接收完成
                // IAP_Packet_t packet;
                // if (IAP_ParsePacket(rx_buffer, rx_index, &packet)) {
                //     // 处理数据包
                //     IAP_ProcessPacket(&packet);
                // } else {
                //     // CRC错误
                //     IAP_SendResponse(rx_buffer[1], RESP_CRC_ERROR);
                // }
                state = 0;
                //rx_index = 0;
            }
            break;

        default:
            state = 0;
            rx_index = 0;
            break;
    }
}