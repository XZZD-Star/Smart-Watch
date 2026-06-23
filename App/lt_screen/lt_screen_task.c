#include "lt_screen_task.h"

#include "cmsis_os2.h"
#include "usart.h"

#define LTSCREEN_BOOT_READY_DELAY_MS 1200U
#define LTSCREEN_POLL_INTERVAL_MS    1000U
#define LTSCREEN_FORMAT_TEST_INTERVAL_MS 3000U

#define LTSCREEN_WRITE_CMD           0x10U
#define LTSCREEN_HEART_RATE_ADDR     0x02B9U
#define LTSCREEN_SPO2_ADDR           0x02CDU
#define LTSCREEN_NET_DEBUG_TEXT_ADDR 0x0000U

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static void lt_screen_run_normal_test(void);
#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
static void lt_screen_run_net_debug_test(void);
#endif

void LTScreenTask_Run(void)
{
  LT168B_TouchEvent_t event;

  LT168B_Init(&huart7);
  osDelay(LTSCREEN_BOOT_READY_DELAY_MS);

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
  lt_screen_run_normal_test();
#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
  lt_screen_run_net_debug_test();
#endif

  for (;;)
  {
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

  switch (event->address)
  {
  default:
    break;
  }
}

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static void lt_screen_run_normal_test(void)
{
  static const uint8_t ascii_zero_value[] = {'8', '8', 0x00U};

  for (;;)
  {
    LT168B_DebugPrintLine("[LT168B TX] ASCII ZERO: 38 38 00");
    LT168B_SendStr(LTSCREEN_WRITE_CMD,
                   LTSCREEN_SPO2_ADDR,
                   ascii_zero_value,
                   (uint8_t)sizeof(ascii_zero_value));

    osDelay(LTSCREEN_FORMAT_TEST_INTERVAL_MS);
  }
}

#elif APP_LTSCREEN_MODE == LTSCREEN_MODE_NET_DEBUG
static void lt_screen_run_net_debug_test(void)
{
  static const uint8_t text[] = "NET DEBUG";

  LT168B_SendStr(LTSCREEN_WRITE_CMD,
                 LTSCREEN_NET_DEBUG_TEXT_ADDR,
                 text,
                 (uint8_t)(sizeof(text) - 1U));
}
#endif
