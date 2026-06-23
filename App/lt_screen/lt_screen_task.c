#include "lt_screen_task.h"

#include "cmsis_os2.h"
#include "usart.h"

#define LTSCREEN_BOOT_READY_DELAY_MS 1200U
#define LTSCREEN_POLL_INTERVAL_MS    20U
#define LTSCREEN_HEALTH_REFRESH_MS   10000U

#define LTSCREEN_TEXT_END_TEST_NONE  0U
#define LTSCREEN_TEXT_END_TEST_00    1U
#define LTSCREEN_TEXT_END_TEST_00_00 2U

#ifndef APP_LTSCREEN_TEXT_END_TEST
//#define APP_LTSCREEN_TEXT_END_TEST LTSCREEN_TEXT_END_TEST_00
#define APP_LTSCREEN_TEXT_END_TEST LTSCREEN_TEXT_END_TEST_NONE
#endif

#if ((APP_LTSCREEN_TEXT_END_TEST != LTSCREEN_TEXT_END_TEST_NONE) && \
     (APP_LTSCREEN_TEXT_END_TEST != LTSCREEN_TEXT_END_TEST_00) && \
     (APP_LTSCREEN_TEXT_END_TEST != LTSCREEN_TEXT_END_TEST_00_00))
#error "APP_LTSCREEN_TEXT_END_TEST must be NONE, 00 or 00_00"
#endif

#define LTSCREEN_HEART_RATE_ADDR     0x02B9U
#define LTSCREEN_SPO2_ADDR           0x02CDU
#define LTSCREEN_NET_DEBUG_TEXT_ADDR 0x0000U

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static void lt_screen_refresh_health_test(void);
static void lt_screen_refresh_health_no_end_test(void);
static void lt_screen_refresh_health_00_test(void);
static void lt_screen_refresh_health_00_00_test(void);
#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
static void lt_screen_run_net_debug_test(void);
#endif

void LTScreenTask_Run(void)
{
  LT168B_TouchEvent_t event;
#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
  uint32_t next_health_refresh_tick;
#endif

  LT168B_Init(&huart7);
  osDelay(LTSCREEN_BOOT_READY_DELAY_MS);

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
  lt_screen_refresh_health_test();
  next_health_refresh_tick = osKernelGetTickCount() + LTSCREEN_HEALTH_REFRESH_MS;
#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
  lt_screen_run_net_debug_test();
#endif

  for (;;)
  {
#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
    if ((int32_t)(osKernelGetTickCount() - next_health_refresh_tick) >= 0)
    {
      lt_screen_refresh_health_test();
      next_health_refresh_tick = osKernelGetTickCount() + LTSCREEN_HEALTH_REFRESH_MS;
    }
#endif

    if (LT168B_TakeTouchEvent(&event) != 0U)
    {
      LTScreen_HandleTouchEvent(&event);
    }

    osDelay(LTSCREEN_POLL_INTERVAL_MS);
  }
}

void LTScreen_HandleTouchEvent(const LT168B_TouchEvent_t *event)
{
  if (event == NULL)
  {
    return;
  }

  LT168B_DebugPrintKeyEvent(event);

  switch (event->address)
  {
  default:
    break;
  }
}

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static void lt_screen_refresh_health_test(void)
{
  switch (APP_LTSCREEN_TEXT_END_TEST)
  {
  case LTSCREEN_TEXT_END_TEST_NONE:
    lt_screen_refresh_health_no_end_test();
    break;

  case LTSCREEN_TEXT_END_TEST_00_00:
    lt_screen_refresh_health_00_00_test();
    break;

  case LTSCREEN_TEXT_END_TEST_00:
  default:
    lt_screen_refresh_health_00_test();
    break;
  }
}

static void lt_screen_refresh_health_no_end_test(void)
{
  static const uint8_t heart_rate[] = {'7', '8'};
  static const uint8_t spo2[] = {'2', '8'};

  LT168B_SendStr(0x10U,
                 LTSCREEN_HEART_RATE_ADDR,
                 heart_rate,
                 (uint8_t)sizeof(heart_rate));
  LT168B_SendStr(0x10U,
                 LTSCREEN_SPO2_ADDR,
                 spo2,
                 (uint8_t)sizeof(spo2));
}

static void lt_screen_refresh_health_00_test(void)
{
  LT168B_WriteText(LTSCREEN_HEART_RATE_ADDR, "78");
  LT168B_WriteText(LTSCREEN_SPO2_ADDR, "18");
}

static void lt_screen_refresh_health_00_00_test(void)
{
  static const uint8_t heart_rate[] = {'7', '8', 0x00U, 0x00U};
  static const uint8_t spo2[] = {'1', '8', 0x00U, 0x00U};

  LT168B_SendStr(0x10U,
                 LTSCREEN_HEART_RATE_ADDR,
                 heart_rate,
                 (uint8_t)sizeof(heart_rate));
  LT168B_SendStr(0x10U,
                 LTSCREEN_SPO2_ADDR,
                 spo2,
                 (uint8_t)sizeof(spo2));
}

#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
static void lt_screen_run_net_debug_test(void)
{
  static const uint8_t text[] = "NET DEBUG";

  LT168B_SendStr(0x10U,
                 LTSCREEN_NET_DEBUG_TEXT_ADDR,
                 text,
                 (uint8_t)(sizeof(text) - 1U));
}
#endif
