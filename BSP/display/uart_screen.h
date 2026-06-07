#ifndef __UART_SCREEN_H
#define __UART_SCREEN_H

#include "stm32h7xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SCREEN_TYPE_NEXTION = 0U
} ScreenType_t;

typedef enum
{
  SCREEN_BAUD_115200 = 115200U
} ScreenBaud_t;

/* 屏幕任务调用：绑定 UART 句柄并记录屏幕类型。 */
void Screen_Init(UART_HandleTypeDef *huart, ScreenType_t type, uint32_t baudrate);
/* 查询屏幕 UART 是否已经初始化。 */
uint8_t Screen_IsReady(void);

/* 底层发送：向屏幕 UART 发送原始字节。 */
void Screen_SendBytes(const uint8_t *data, uint16_t len);
/* 底层发送：向屏幕 UART 发送普通字符串。 */
void Screen_SendString(const char *str);
/* Nextion 命令发送：自动补齐命令结束符。 */
void Screen_SendCommand(const char *cmd);

/* Nextion 页面切换。 */
void Screen_Nextion_SetPage(uint8_t page_id);
/* Nextion 当前页查询。 */
void Screen_Nextion_RequestPageId(void);
uint8_t Screen_Nextion_TakeLatestPageId(uint8_t *page_id);
void Screen_Nextion_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size);
/* Nextion 文本控件更新。 */
void Screen_Nextion_SetText(const char *component, const char *text);
/* Nextion 数值控件更新。 */
void Screen_Nextion_SetValue(const char *component, int32_t value);
/* Nextion 图片控件更新。 */
void Screen_Nextion_SetPicture(const char *component, uint16_t picture_id);
/* Nextion 控件强制刷新。 */
void Screen_Nextion_RefreshComponent(const char *component);

#ifdef __cplusplus
}
#endif

#endif /* __UART_SCREEN_H */
