#ifndef CLOUD_TASK_H
#define CLOUD_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS Task2 入口调用：维护 ESP8266/OneNet 连接、订阅、上报和下发处理。 */
void CloudTask_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_TASK_H */
