#ifndef __DEBUG_UART7_H
#define __DEBUG_UART7_H

#include <stdint.h>

/* 调试串口输出 printf 风格日志，UART7 非调试角色时为空实现。 */
void Debug_Printf(const char *fmt, ...);
/* 打印 ESP8266 AT/TCP 发送内容。 */
void Debug_LogEspTx(const char *line);
/* 打印 ESP8266 AT/TCP 接收内容。 */
void Debug_LogEspRx(const uint8_t *data, uint16_t len);
/* 打印融合帧中的生理数据摘要。 */
void Debug_LogBio(int32_t heart_rate, int32_t spo2, int8_t hr_valid, int8_t spo2_valid);

#endif /* __DEBUG_UART7_H */
