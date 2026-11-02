#include "usart.h"

#include <string.h>
#include <stdint.h>

extern UART_HandleTypeDef huart3;

HAL_StatusTypeDef ESP8266_SendAT(const char *cmd, uint32_t timeout)
{
    return HAL_UART_Transmit(
        &huart3,
        (const uint8_t *)cmd,
        (uint16_t)strlen(cmd),
        HAL_MAX_DELAY
    );
}

// HAL_StatusTypeDef ESP8266_Init(void)
// {
//     ESP8266_SendAT("AT\r\n",);
//     //UART3_SendAT("AT+RST\r\n");
//     ESP8266_SendAT("AT+GMR\r\n");
//     ESP8266_SendAT("AT+CWMODE=0\r\n");
//     ESP8266_SendAT("AT+CWJAP=Xiaomi 14 Ultra,123456789\r\n");
//     ESP8266_SendAT("AT+MQTTCONN=broker.emqx.io,1883,1\r\n");
//     ESP8266_SendAT("AT+MQTTCONN?\r\n");
//     ESP8266_SendAT("AT+MQTTPUBRAW=wf24,12,0,0\r\n");
// }