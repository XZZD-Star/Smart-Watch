#include "uart_screen.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  UART_HandleTypeDef *huart;
  ScreenType_t type;
  uint32_t baudrate;
} ScreenContext_t;

static ScreenContext_t g_screen_ctx = {0};

#define SCREEN_UART_TX_TIMEOUT_MS 15U
#define SCREEN_UART7_RX_BUFFER_SIZE 32U
#define SCREEN_NEXTION_END_SIZE 3U
#define SCREEN_NEXTION_END_BYTE 0xFFU
#define SCREEN_NEXTION_PAGE_ID_EVENT 0x66U
#define SCREEN_NEXTION_COMMAND_BUFFER_SIZE 128U

static uint8_t g_nextion_page_rx_buffer[SCREEN_UART7_RX_BUFFER_SIZE];
static uint8_t g_nextion_page_rx_state = 0U;
static uint8_t g_nextion_page_rx_id = 0U;
static uint8_t g_nextion_page_rx_end_count = 0U;
static volatile uint8_t g_nextion_latest_page_id = 0U;
static volatile uint8_t g_nextion_latest_page_pending = 0U;

static uint8_t Screen_ParseNextionPageIdByte(uint8_t byte, uint8_t *page_id)
{
  if (page_id == NULL)
  {
    return 0U;
  }

  switch (g_nextion_page_rx_state)
  {
  case 0U:
    if (byte == SCREEN_NEXTION_PAGE_ID_EVENT)
    {
      g_nextion_page_rx_state = 1U;
    }
    break;
  case 1U:
    g_nextion_page_rx_id = byte;
    g_nextion_page_rx_end_count = 0U;
    g_nextion_page_rx_state = 2U;
    break;
  default:
    if (byte == SCREEN_NEXTION_END_BYTE)
    {
      g_nextion_page_rx_end_count++;
      if (g_nextion_page_rx_end_count >= SCREEN_NEXTION_END_SIZE)
      {
        *page_id = g_nextion_page_rx_id;
        g_nextion_page_rx_state = 0U;
        g_nextion_page_rx_end_count = 0U;
        return 1U;
      }
    }
    else
    {
      g_nextion_page_rx_state =
        (byte == SCREEN_NEXTION_PAGE_ID_EVENT) ? 1U : 0U;
      g_nextion_page_rx_end_count = 0U;
    }
    break;
  }

  return 0U;
}

static void Screen_Nextion_RestartPageIdRx(void)
{
  if ((Screen_IsReady() == 0U) || (g_screen_ctx.type != SCREEN_TYPE_NEXTION))
  {
    return;
  }

  (void)HAL_UARTEx_ReceiveToIdle_IT(g_screen_ctx.huart,
                                    g_nextion_page_rx_buffer,
                                    sizeof(g_nextion_page_rx_buffer));
}

static void Screen_SendNextionCommand(const char *cmd)
{
  uint8_t payload[SCREEN_NEXTION_COMMAND_BUFFER_SIZE];
  size_t cmd_len;
  size_t payload_len;

  if (cmd == NULL)
  {
    return;
  }

  cmd_len = strlen(cmd);
  payload_len = cmd_len + SCREEN_NEXTION_END_SIZE;
  if (payload_len > sizeof(payload))
  {
    return;
  }

  (void)memcpy(payload, cmd, cmd_len);
  payload[cmd_len] = 0xFFU;
  payload[cmd_len + 1U] = 0xFFU;
  payload[cmd_len + 2U] = 0xFFU;

  Screen_SendBytes(payload, (uint16_t)payload_len);
}

void Screen_Init(UART_HandleTypeDef *huart, ScreenType_t type, uint32_t baudrate)
{
  g_screen_ctx.huart = huart;
  g_screen_ctx.type = type;
  g_screen_ctx.baudrate = baudrate;

  (void)g_screen_ctx.baudrate;

  Screen_Nextion_RestartPageIdRx();
}

uint8_t Screen_IsReady(void)
{
  return ((g_screen_ctx.huart != NULL) && (g_screen_ctx.huart->Instance != NULL)) ? 1U : 0U;
}

void Screen_SendBytes(const uint8_t *data, uint16_t len)
{
  if ((Screen_IsReady() == 0U) || (data == NULL) || (len == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(g_screen_ctx.huart, (uint8_t *)data, len, SCREEN_UART_TX_TIMEOUT_MS);
}

void Screen_SendString(const char *str)
{
  if (str == NULL)
  {
    return;
  }

  Screen_SendBytes((const uint8_t *)str, (uint16_t)strlen(str));
}

void Screen_SendCommand(const char *cmd)
{
  if (cmd == NULL)
  {
    return;
  }

  if (g_screen_ctx.type == SCREEN_TYPE_NEXTION)
  {
    Screen_SendNextionCommand(cmd);
    return;
  }

  Screen_SendString(cmd);
}

void Screen_Nextion_SetPage(uint8_t page_id)
{
  char command[24];

  (void)snprintf(command, sizeof(command), "page %u", (unsigned int)page_id);
  Screen_SendCommand(command);
}

void Screen_Nextion_RequestPageId(void)
{
  Screen_SendCommand("sendme");
}

uint8_t Screen_Nextion_TakeLatestPageId(uint8_t *page_id)
{
  if (page_id == NULL)
  {
    return 0U;
  }

  if (g_nextion_latest_page_pending == 0U)
  {
    return 0U;
  }

  g_nextion_latest_page_pending = 0U;
  *page_id = g_nextion_latest_page_id;

  return 1U;
}

void Screen_Nextion_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
  uint16_t index;
  uint8_t page_id = 0U;

  if ((Screen_IsReady() == 0U) ||
      (huart != g_screen_ctx.huart) ||
      (g_screen_ctx.type != SCREEN_TYPE_NEXTION))
  {
    return;
  }

  if (size > sizeof(g_nextion_page_rx_buffer))
  {
    size = sizeof(g_nextion_page_rx_buffer);
  }

  for (index = 0U; index < size; index++)
  {
    if (Screen_ParseNextionPageIdByte(g_nextion_page_rx_buffer[index],
                                      &page_id) != 0U)
    {
      g_nextion_latest_page_id = page_id;
      g_nextion_latest_page_pending = 1U;
    }
  }

  Screen_Nextion_RestartPageIdRx();
}

void Screen_Nextion_SetText(const char *component, const char *text)
{
  char command[96];

  if ((component == NULL) || (text == NULL))
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.txt=\"%s\"", component, text);
  Screen_SendCommand(command);
}

void Screen_Nextion_SetValue(const char *component, int32_t value)
{
  char command[64];

  if (component == NULL)
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.val=%ld", component, (long)value);
  Screen_SendCommand(command);
}

void Screen_Nextion_SetPicture(const char *component, uint16_t picture_id)
{
  char command[64];

  if (component == NULL)
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.pic=%u", component, (unsigned int)picture_id);
  Screen_SendCommand(command);
}

void Screen_Nextion_RefreshComponent(const char *component)
{
  char command[64];

  if (component == NULL)
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "ref %s", component);
  Screen_SendCommand(command);
}
