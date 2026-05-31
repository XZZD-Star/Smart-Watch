#ifndef __ESP8266_H__
#define __ESP8266_H__

#include <stdint.h>

#define WIFI_SSID              "www"
#define WIFI_PASSWORD          "www123456"

#define ONENET_HOST            "mqtts.heclouds.com"
#define ONENET_PORT            1883U

#define ESP8266_INIT_STATUS_OK            0U
#define ESP8266_INIT_STATUS_FAIL_AT       1U
#define ESP8266_INIT_STATUS_FAIL_ATE0     2U
#define ESP8266_INIT_STATUS_FAIL_CWMODE   3U
#define ESP8266_INIT_STATUS_FAIL_DHCP     4U
#define ESP8266_INIT_STATUS_FAIL_CWJAP    5U
#define ESP8266_INIT_STATUS_FAIL_CIPMUX   6U
#define ESP8266_INIT_STATUS_FAIL_CIPSTART 7U

/* 清空 ESP8266 软件接收缓存，网络任务重连或处理完 +IPD 后调用。 */
void ESP8266_Clear(void);
/* 等待接收缓存出现数据，主要供 AT 命令等待流程使用。 */
uint8_t ESP8266_WaitReceive(uint32_t timeout_ms);
/* 发送一条 AT 命令并等待期望字符串。 */
uint8_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms);
/* 通过当前 TCP 连接发送 MQTT 原始数据。 */
uint8_t ESP8266_SendData(const uint8_t *data, uint16_t len);
/* 网络任务调用：从接收缓存中取完整 +IPD payload。 */
uint8_t *ESP8266_GetIPD(uint32_t timeout_ms);
/* 初始化 WiFi、TCP 连接和 ESP8266 基本 AT 配置。 */
uint8_t ESP8266_Init(void);

/* 兼容旧逐字节接收路径：向软件缓存写入一个字节。 */
void ESP8266_RxFeedByte(uint8_t byte);
/* USART6 DMA 回调调用：将一段接收数据写入软件缓存。 */
void ESP8266_RxFeedBlock(const uint8_t *data, uint16_t len);
/* 查询是否检测到 CLOSED/WIFI DISCONNECT 等传输错误。 */
uint8_t ESP8266_HasTransportError(void);
/* 清除传输错误标志，重连前调用。 */
void ESP8266_ClearTransportError(void);
/* 查询最近一次初始化失败原因。 */
uint8_t ESP8266_GetLastInitStatus(void);
/* 调试用：打印当前接收缓存前缀。 */
void ESP8266_DebugDumpCurrentRx(const char *tag, uint16_t limit);

#endif /* __ESP8266_H__ */
