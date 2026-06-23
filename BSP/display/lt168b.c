#include "lt168b.h"

#include "usart.h"

#include <string.h>

#define LT168B_UART_TX_TIMEOUT_MS 20U
#define LT168B_RX_BUFFER_SIZE     64U

#define LT168B_FRAME_HEADER_0     0x5AU
#define LT168B_FRAME_HEADER_1     0xA5U
#define LT168B_TX_FRAME_OVERHEAD  8U
#define LT168B_LEN_WITHOUT_DATA   5U
#define LT168B_MAX_DATA_LEN       (255U - LT168B_LEN_WITHOUT_DATA)

#define LT168B_WRITE_CMD          0x10U
#define LT168B_TEXT_END_LEN       2U
#define LT168B_TOUCH_CMD          0x41U
#define LT168B_TOUCH_LEN          0x07U
#define LT168B_TOUCH_FRAME_LEN    10U
#define LT168B_CRC_INIT           0xFFFFU
#define LT168B_CRC_POLY           0xA001U

static UART_HandleTypeDef *g_lt168b_huart = NULL;
static uint8_t g_lt168b_rx_buffer[LT168B_RX_BUFFER_SIZE];
static LT168B_TouchEvent_t g_lt168b_touch_event;
static volatile uint8_t g_lt168b_touch_pending = 0U;

static void LT168B_RestartRx(void);
static uint16_t LT168B_CalcCrc16Modbus(const uint8_t *data, uint16_t len);
static void LT168B_DebugUart2WriteByte(uint8_t byte);
static void LT168B_DebugUart2WriteString(const char *text);
static void LT168B_DebugUart2WriteDecU16(uint16_t value);
static void LT168B_DebugUart2WriteHexU8(uint8_t value);
static void LT168B_DebugUart2WriteHexU16(uint16_t value);
static void LT168B_DebugPrintRawFrame(const uint8_t *data, uint16_t len);

void LT168B_Init(UART_HandleTypeDef *huart)
{
  g_lt168b_huart = huart;
  LT168B_RestartRx();
}

void LT168B_SendStr(uint8_t cmd, uint16_t address, const uint8_t *data, uint8_t len)
{
  uint8_t frame[LT168B_TX_FRAME_OVERHEAD + 255U];
  uint16_t crc;
  uint16_t index = 0U;
  uint16_t crc_start_index;

  if ((g_lt168b_huart == NULL) ||
      (g_lt168b_huart->Instance == NULL) ||
      (data == NULL) ||
      (len == 0U) ||
      (len > LT168B_MAX_DATA_LEN))
  {
    return;
  }

  frame[index++] = LT168B_FRAME_HEADER_0;
  frame[index++] = LT168B_FRAME_HEADER_1;
  frame[index++] = (uint8_t)(LT168B_LEN_WITHOUT_DATA + len);

  crc_start_index = index;
  frame[index++] = cmd;
  frame[index++] = (uint8_t)(address >> 8);
  frame[index++] = (uint8_t)(address & 0xFFU);

  (void)memcpy(&frame[index], data, len);
  index = (uint16_t)(index + len);

  crc = LT168B_CalcCrc16Modbus(&frame[crc_start_index],
                               (uint16_t)(3U + len));
  frame[index++] = (uint8_t)(crc & 0xFFU);
  frame[index++] = (uint8_t)(crc >> 8);

  (void)HAL_UART_Transmit(g_lt168b_huart, frame, index, LT168B_UART_TX_TIMEOUT_MS);
}

void LT168B_WriteText(uint16_t address, const char *text)
{
  uint8_t data[LT168B_MAX_DATA_LEN];
  uint16_t text_len;

  if (text == NULL)
  {
    return;
  }

  text_len = (uint16_t)strlen(text);
  if (text_len > (uint16_t)(LT168B_MAX_DATA_LEN - LT168B_TEXT_END_LEN))
  {
    text_len = (uint16_t)(LT168B_MAX_DATA_LEN - LT168B_TEXT_END_LEN);
  }

  (void)memcpy(data, text, text_len);
  data[text_len] = 0x00U;
  data[text_len + 1U] = 0x00U;

  LT168B_SendStr(LT168B_WRITE_CMD,
                 address,
                 data,
                 (uint8_t)(text_len + LT168B_TEXT_END_LEN));
}

void LT168B_WriteU16(uint16_t address, uint16_t value)
{
  uint8_t data[2];

  data[0] = (uint8_t)(value >> 8);
  data[1] = (uint8_t)(value & 0xFFU);

  LT168B_SendStr(LT168B_WRITE_CMD, address, data, (uint8_t)sizeof(data));
}

void LT168B_DebugPrintLine(const char *text)
{
  LT168B_DebugUart2WriteString(text);
  LT168B_DebugUart2WriteString("\r\n");
}

void LT168B_DebugPrintKeyEvent(const LT168B_TouchEvent_t *event)
{
  if (event == NULL)
  {
    return;
  }

  LT168B_DebugUart2WriteString("[LT168B KEY] addr=0x");
  LT168B_DebugUart2WriteHexU16(event->address);
  LT168B_DebugUart2WriteString(" key=0x");
  LT168B_DebugUart2WriteHexU16(event->key_value);
  LT168B_DebugUart2WriteString("\r\n");
}

uint8_t LT168B_ParseTouchEvent(const uint8_t *frame,
                               uint16_t frame_len,
                               LT168B_TouchEvent_t *event)
{
  uint8_t len;
  uint16_t crc_calc;
  uint16_t crc_frame;

  if ((frame == NULL) || (event == NULL))
  {
    return 0U;
  }

  if (frame_len < LT168B_TOUCH_FRAME_LEN)
  {
    return 0U;
  }

  if ((frame[0] != LT168B_FRAME_HEADER_0) || (frame[1] != LT168B_FRAME_HEADER_1))
  {
    return 0U;
  }

  len = frame[2];
  if (frame_len != (uint16_t)(3U + len))
  {
    return 0U;
  }

  if ((len != LT168B_TOUCH_LEN) || (frame[3] != LT168B_TOUCH_CMD))
  {
    return 0U;
  }

  crc_calc = LT168B_CalcCrc16Modbus(&frame[3], (uint16_t)(len - 2U));
  crc_frame = (uint16_t)frame[8] | ((uint16_t)frame[9] << 8);
  if (crc_calc != crc_frame)
  {
    return 0U;
  }

  event->address = ((uint16_t)frame[4] << 8) | frame[5];
  event->key_value = ((uint16_t)frame[6] << 8) | frame[7];

  return 1U;
}

void LT168B_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
  LT168B_TouchEvent_t event;

  if (huart != g_lt168b_huart)
  {
    return;
  }

  if (size == 0U)
  {
    LT168B_RestartRx();
    return;
  }

  if (size > LT168B_RX_BUFFER_SIZE)
  {
    size = LT168B_RX_BUFFER_SIZE;
  }

  LT168B_DebugPrintRawFrame(g_lt168b_rx_buffer, size);

  if (LT168B_ParseTouchEvent(g_lt168b_rx_buffer, size, &event) != 0U)
  {
    g_lt168b_touch_event = event;
    g_lt168b_touch_pending = 1U;
  }

  LT168B_RestartRx();
}

uint8_t LT168B_TakeTouchEvent(LT168B_TouchEvent_t *event)
{
  if ((event == NULL) || (g_lt168b_touch_pending == 0U))
  {
    return 0U;
  }

  *event = g_lt168b_touch_event;
  g_lt168b_touch_pending = 0U;

  return 1U;
}

static void LT168B_RestartRx(void)
{
  if ((g_lt168b_huart == NULL) || (g_lt168b_huart->Instance == NULL))
  {
    return;
  }

  (void)HAL_UARTEx_ReceiveToIdle_IT(g_lt168b_huart,
                                    g_lt168b_rx_buffer,
                                    sizeof(g_lt168b_rx_buffer));
}

static uint16_t LT168B_CalcCrc16Modbus(const uint8_t *data, uint16_t len)
{
  uint16_t crc = LT168B_CRC_INIT;
  uint16_t index;
  uint8_t bit;

  for (index = 0U; index < len; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (uint16_t)((crc >> 1) ^ LT168B_CRC_POLY);
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

static void LT168B_DebugUart2WriteByte(uint8_t byte)
{
  if (huart2.Instance == NULL)
  {
    return;
  }

  while ((huart2.Instance->ISR & 0x40U) == 0U)
  {
  }
  huart2.Instance->TDR = byte;
}

static void LT168B_DebugUart2WriteString(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  while (*text != '\0')
  {
    LT168B_DebugUart2WriteByte((uint8_t)*text);
    text++;
  }
}

static void LT168B_DebugUart2WriteDecU16(uint16_t value)
{
  char digits[5];
  uint8_t count = 0U;

  if (value == 0U)
  {
    LT168B_DebugUart2WriteByte((uint8_t)'0');
    return;
  }

  while ((value > 0U) && (count < sizeof(digits)))
  {
    digits[count++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  while (count > 0U)
  {
    count--;
    LT168B_DebugUart2WriteByte((uint8_t)digits[count]);
  }
}

static void LT168B_DebugUart2WriteHexU8(uint8_t value)
{
  static const char hex[] = "0123456789ABCDEF";

  LT168B_DebugUart2WriteByte((uint8_t)hex[(value >> 4) & 0x0FU]);
  LT168B_DebugUart2WriteByte((uint8_t)hex[value & 0x0FU]);
}

static void LT168B_DebugUart2WriteHexU16(uint16_t value)
{
  LT168B_DebugUart2WriteHexU8((uint8_t)(value >> 8));
  LT168B_DebugUart2WriteHexU8((uint8_t)(value & 0xFFU));
}

static void LT168B_DebugPrintRawFrame(const uint8_t *data, uint16_t len)
{
  uint16_t index;

  if (data == NULL)
  {
    return;
  }

  LT168B_DebugUart2WriteString("[LT168B RX] len=");
  LT168B_DebugUart2WriteDecU16(len);
  LT168B_DebugUart2WriteString(" data=");

  for (index = 0U; index < len; index++)
  {
    if (index > 0U)
    {
      LT168B_DebugUart2WriteByte((uint8_t)' ');
    }
    LT168B_DebugUart2WriteHexU8(data[index]);
  }

  LT168B_DebugUart2WriteString("\r\n");
}
