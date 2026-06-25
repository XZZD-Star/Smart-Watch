#include "lt_screen_task.h"

#include "cmsis_os2.h"
#include "cloud_task.h"
#include "debug_uart7.h"
#include "lt_screen_keys.h"
#include "ota_service.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define LTSCREEN_BOOT_READY_DELAY_MS 1200U
#define LTSCREEN_POLL_INTERVAL_MS    20U
#define LTSCREEN_HEALTH_REFRESH_MS   1000U
#define LTSCREEN_VERSION_PAGE_DELAY_MS 50U
#define LTSCREEN_UPDATE_STEP_DELAY_MS 350U
#define LTSCREEN_OTA_EXCLUSIVE_TIMEOUT_MS 10000U

#define LTSCREEN_HEART_RATE_ADDR     0x02B9U
#define LTSCREEN_SPO2_ADDR           0x02CDU
#define LTSCREEN_FALL_ADDR           0x02E1U
#define LTSCREEN_CURRENT_VERSION_ADDR 0x0227U
#define LTSCREEN_LATEST_VERSION_ADDR  0x01C3U
#define LTSCREEN_UPDATE_PROGRESS_ADDR 0x02A0U
#define LTSCREEN_NET_DEBUG_TEXT_ADDR 0x0000U

#define LTSCREEN_VERSION_SAME_PAGE_ID   0x0005U
#define LTSCREEN_VERSION_UPDATE_PAGE_ID 0x0002U
#define LTSCREEN_UPDATE_PROGRESS_MAX    0x0010U
#define LTSCREEN_CURRENT_VERSION_TEXT_TEST "1.2"
#define LTSCREEN_LATEST_VERSION_TEXT_TEST  "1.3"

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static OTAService_TaskInfo_t s_lt_screen_ota_task;
static uint8_t s_lt_screen_has_ota_task = 0U;
static uint8_t s_lt_screen_ota_demo_active = 0U;

static void lt_screen_refresh_health_test(void);
static void lt_screen_send_version_texts(const char *current_version,
                                         const char *latest_version);
static uint8_t lt_screen_is_key(const LT168B_TouchEvent_t *event, uint16_t key_value);
static uint8_t lt_screen_enter_ota_demo(void);
static void lt_screen_leave_ota_demo(void);
static void lt_screen_handle_version_query(void);
static void lt_screen_handle_update_start(void);
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
    if ((s_lt_screen_ota_demo_active == 0U) &&
        ((int32_t)(osKernelGetTickCount() - next_health_refresh_tick) >= 0))
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

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
  if ((s_lt_screen_ota_demo_active != 0U) &&
      (lt_screen_is_key(event, LTSCREEN_KEY_BACK) != 0U))
  {
    lt_screen_leave_ota_demo();
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_VERSION_UPDATE) != 0U)
  {
    lt_screen_handle_version_query();
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_UPDATE_START) != 0U)
  {
    lt_screen_handle_update_start();
    return;
  }
#endif

  switch (event->address)
  {
  default:
    break;
  }
}

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static void lt_screen_refresh_health_test(void)
{
  static const uint8_t heart_rate[] = {'7', '8'};
  static const uint8_t spo2[] = {'9', '8'};
  /* GBK 编码：危险，用于判断跌倒控件是否支持字符串显示。 */
  static const uint8_t fall_status[] = {0xCEU, 0xA3U, 0xCFU, 0xD5U};

  LT168B_SendStr(0x10U,
                 LTSCREEN_HEART_RATE_ADDR,
                 heart_rate,
                 (uint8_t)sizeof(heart_rate));
  LT168B_SendStr(0x10U,
                 LTSCREEN_SPO2_ADDR,
                 spo2,
                 (uint8_t)sizeof(spo2));
  LT168B_SendStr(0x10U,
                 LTSCREEN_FALL_ADDR,
                 fall_status,
                 (uint8_t)sizeof(fall_status));
}

static void lt_screen_send_version_texts(const char *current_version,
                                         const char *latest_version)
{
  LT168B_WriteText(LTSCREEN_CURRENT_VERSION_ADDR, current_version);
  LT168B_WriteText(LTSCREEN_LATEST_VERSION_ADDR, latest_version);
}

static uint8_t lt_screen_is_key(const LT168B_TouchEvent_t *event, uint16_t key_value)
{
  if (event == NULL)
  {
    return 0U;
  }

  if ((event->address == key_value) || (event->key_value == key_value))
  {
    return 1U;
  }

  return 0U;
}

static uint8_t lt_screen_enter_ota_demo(void)
{
  if (s_lt_screen_ota_demo_active != 0U)
  {
    return 1U;
  }

  if (CloudTask_RequestOtaExclusive(LTSCREEN_OTA_EXCLUSIVE_TIMEOUT_MS) == 0U)
  {
    LT168B_DebugPrintLine("[OTA SCREEN] cloud pause timeout");
    return 0U;
  }

  s_lt_screen_ota_demo_active = 1U;
  LT168B_DebugPrintLine("[OTA SCREEN] exclusive ready");
  return 1U;
}

static void lt_screen_leave_ota_demo(void)
{
  if (s_lt_screen_ota_demo_active == 0U)
  {
    return;
  }

  memset(&s_lt_screen_ota_task, 0, sizeof(s_lt_screen_ota_task));
  s_lt_screen_has_ota_task = 0U;
  s_lt_screen_ota_demo_active = 0U;
  CloudTask_ReleaseOtaExclusive();
  LT168B_DebugPrintLine("[OTA SCREEN] leave demo");
  Debug_Printf("[OTA SCREEN] leave demo\r\n");
}

static void lt_screen_handle_version_query(void)
{
  OTAService_TaskInfo_t task;
  char current_version[OTA_TARGET_VERSION_LEN];
  char latest_version[OTA_TARGET_VERSION_LEN];
  int result;

  LT168B_DebugPrintLine("[OTA SCREEN] query button");
  if (lt_screen_enter_ota_demo() == 0U)
  {
    return;
  }

  LT168B_DebugPrintLine("[OTA SCREEN] query start");
  result = OTAService_QueryTask(&task,
                                current_version,
                                (uint32_t)sizeof(current_version),
                                latest_version,
                                (uint32_t)sizeof(latest_version));

  if (result == OTA_SERVICE_UPDATED)
  {
    s_lt_screen_ota_task = task;
    s_lt_screen_has_ota_task = 1U;
    LT168B_DebugPrintLine("[OTA SCREEN] query updated");
    LT168B_DebugPrintLine("[OTA SCREEN] latest:");
    LT168B_DebugPrintLine(latest_version);
    lt_screen_send_version_texts(LTSCREEN_CURRENT_VERSION_TEXT_TEST,
                                 LTSCREEN_LATEST_VERSION_TEXT_TEST);
    LT168B_GotoPage(LTSCREEN_VERSION_UPDATE_PAGE_ID);
    return;
  }

  memset(&s_lt_screen_ota_task, 0, sizeof(s_lt_screen_ota_task));
  s_lt_screen_has_ota_task = 0U;
  if (result == OTA_SERVICE_ERROR)
  {
    LT168B_DebugPrintLine("[OTA SCREEN] query failed");
    lt_screen_leave_ota_demo();
    return;
  }
  LT168B_DebugPrintLine("[OTA SCREEN] query no task");
  LT168B_GotoPage(LTSCREEN_VERSION_SAME_PAGE_ID);
}

static void lt_screen_handle_update_start(void)
{
  char target_version[OTA_TARGET_VERSION_LEN];
  int report_result;
  uint16_t progress;

  if (s_lt_screen_has_ota_task == 0U)
  {
    LT168B_DebugPrintLine("[OTA SCREEN] no OTA task");
    LT168B_GotoPage(LTSCREEN_VERSION_SAME_PAGE_ID);
    return;
  }

  (void)snprintf(target_version, sizeof(target_version), "%s", s_lt_screen_ota_task.target);
  LT168B_WriteU16(LTSCREEN_UPDATE_PROGRESS_ADDR, 0U);
  Debug_Printf("[OTA SCREEN] simulate progress=0\r\n");
  for (progress = 1U; progress <= LTSCREEN_UPDATE_PROGRESS_MAX; progress++)
  {
    osDelay(LTSCREEN_UPDATE_STEP_DELAY_MS);
    LT168B_WriteU16(LTSCREEN_UPDATE_PROGRESS_ADDR, progress);
    Debug_Printf("[OTA SCREEN] simulate progress=%u\r\n", (unsigned int)progress);
  }

  if (OTAService_StartSimulateUpdate(&s_lt_screen_ota_task) != OTA_SERVICE_UPDATED)
  {
    LT168B_DebugPrintLine("[OTA SCREEN] simulate flag failed");
    lt_screen_leave_ota_demo();
    return;
  }

  report_result = OTAService_ReportPendingSimulateVersion();
  Debug_Printf("[OTA SCREEN] report version=%s %s\r\n",
               target_version,
               (report_result == OTA_SERVICE_UPDATED) ? "ok" : "fail");
  memset(&s_lt_screen_ota_task, 0, sizeof(s_lt_screen_ota_task));
  s_lt_screen_has_ota_task = 0U;
  lt_screen_send_version_texts(LTSCREEN_LATEST_VERSION_TEXT_TEST,
                               LTSCREEN_LATEST_VERSION_TEXT_TEST);
  LT168B_GotoPage(LTSCREEN_VERSION_SAME_PAGE_ID);
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
