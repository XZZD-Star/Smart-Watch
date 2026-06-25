#ifndef CLOUD_TASK_H
#define CLOUD_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS Task2 入口调用：维护 ESP8266/OneNet 连接、订阅、上报和下发处理。 */
void CloudTask_Run(void);
/* 屏幕 OTA 演示调用：请求 CloudTask 在安全点释放 ESP8266。 */
uint8_t CloudTask_RequestOtaExclusive(uint32_t timeout_ms);
/* 屏幕 OTA 演示调用：释放 ESP8266 独占，允许 CloudTask 恢复 MQTT。 */
void CloudTask_ReleaseOtaExclusive(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_TASK_H */
