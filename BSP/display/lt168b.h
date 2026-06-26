#ifndef __LT168B_H
#define __LT168B_H

#include "stm32h7xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t address;
  uint16_t key_value;
} LT168B_TouchEvent_t;

void LT168B_Init(UART_HandleTypeDef *huart);
void LT168B_SendStr(uint8_t cmd, uint16_t address, const uint8_t *data, uint8_t len);
void LT168B_WriteText(uint16_t address, const char *text);
HAL_StatusTypeDef LT168B_WriteVersionText(uint16_t address, const char *text);
void LT168B_WriteU16(uint16_t address, uint16_t value);
void LT168B_GotoPage(uint16_t page_id);
void LT168B_DebugPrintLine(const char *text);
void LT168B_DebugPrintKeyEvent(const LT168B_TouchEvent_t *event);
uint8_t LT168B_ParseTouchEvent(const uint8_t *frame,
                               uint16_t frame_len,
                               LT168B_TouchEvent_t *event);
void LT168B_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size);
uint8_t LT168B_TakeTouchEvent(LT168B_TouchEvent_t *event);

#ifdef __cplusplus
}
#endif

#endif /* __LT168B_H */
