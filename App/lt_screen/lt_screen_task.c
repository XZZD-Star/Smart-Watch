#include "lt_screen_task.h"

#include "cmsis_os2.h"
#include "cloud_task.h"
#include "debug_uart7.h"
#include "lt_screen_keys.h"
#include "motion_ai.h"
#include "motion_app_events.h"
#include "motion_input.h"
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
#define LTSCREEN_CALIBRATION_STATUS_ADDR 0x010CU
#define LTSCREEN_TRAINING_FRONT_RAISE_ADDR   0x03E7U
#define LTSCREEN_TRAINING_SIDE_RAISE_ADDR     0x03FBU
#define LTSCREEN_TRAINING_SHOULDER_RAISE_ADDR 0x040FU
#define LTSCREEN_TRAINING_ELBOW_FLEX_ADDR     0x0423U
#define LTSCREEN_CURRENT_VERSION_ADDR 0x0227U
#define LTSCREEN_LATEST_VERSION_ADDR  0x01C3U
#define LTSCREEN_UPDATE_PROGRESS_ADDR 0x02A0U
#define LTSCREEN_DEVICE_DOOR_ADDR     0x00F7U
#define LTSCREEN_NET_DEBUG_TEXT_ADDR 0x0000U

#define LTSCREEN_VERSION_SAME_PAGE_ID   0x0005U
#define LTSCREEN_VERSION_UPDATE_PAGE_ID 0x0002U
#define LTSCREEN_UPDATE_PROGRESS_MAX    0x0010U
#define LTSCREEN_CURRENT_VERSION_TEXT_TEST "1.2"
#define LTSCREEN_LATEST_VERSION_TEXT_TEST  "1.3"
#define LTSCREEN_CALIBRATION_DELAY_MS   1000U
#define LTSCREEN_TRAINING_COUNT_MAX     99U
#define LTSCREEN_TRAINING_ACTION_COUNT  4U

#if APP_LTSCREEN_MODE == LTSCREEN_MODE_NORMAL
static OTAService_TaskInfo_t s_lt_screen_ota_task;
static uint8_t s_lt_screen_has_ota_task = 0U;
static uint8_t s_lt_screen_ota_demo_active = 0U;
static uint8_t s_device_door_open = 0U;
static uint8_t s_device_door_icon_applied = 0U;
static uint8_t s_lt_screen_calibration_done = 0U;
static uint8_t s_lt_screen_training_page_entered = 0U;
static uint8_t s_lt_screen_training_active = 0U;
static uint8_t s_lt_screen_training_count[LTSCREEN_TRAINING_ACTION_COUNT] = {0U, 0U, 0U, 0U};
static uint8_t s_lt_screen_training_last_written[LTSCREEN_TRAINING_ACTION_COUNT] =
  {0xFFU, 0xFFU, 0xFFU, 0xFFU};

static void lt_screen_refresh_health_test(void);
static void lt_screen_send_version_texts(const char *current_version,
                                         const char *latest_version);
static void lt_screen_apply_device_door_icon(uint8_t is_open);
static void lt_screen_reset_training_counts(void);
static void lt_screen_write_training_count(uint16_t address, uint8_t count);
static void lt_screen_sync_training_count(uint8_t action_index);
static void lt_screen_sync_training_counts(void);
static int8_t lt_screen_find_training_action_index(int32_t action_label);
static void lt_screen_process_training_refresh(void);
static void lt_screen_handle_calibration_start(void);
static void lt_screen_handle_training_page_enter(void);
static void lt_screen_handle_training_start(void);
static void lt_screen_handle_training_stop(void);
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
  LTScreen_SetDeviceDoorState(0U);
  s_lt_screen_calibration_done = 0U;
  s_lt_screen_training_page_entered = 0U;
  s_lt_screen_training_active = 0U;
  lt_screen_reset_training_counts();
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

    if ((s_lt_screen_ota_demo_active == 0U) &&
        (s_lt_screen_training_active != 0U))
    {
      lt_screen_process_training_refresh();
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

  if (lt_screen_is_key(event, LTSCREEN_KEY_OPEN_DEVICE) != 0U)
  {
    LTScreen_SetDeviceDoorState(1U);
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_START_CALIBRATION) != 0U)
  {
    lt_screen_handle_calibration_start();
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_START_TRAINING) != 0U)
  {
    lt_screen_handle_training_page_enter();
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_INNER_START_TRAIN) != 0U)
  {
    lt_screen_handle_training_start();
    return;
  }

  if (lt_screen_is_key(event, LTSCREEN_KEY_TRAINING_BACK) != 0U)
  {
    lt_screen_handle_training_stop();
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
void LTScreen_SetDeviceDoorState(uint8_t is_open)
{
  lt_screen_apply_device_door_icon(is_open);
}
#else
void LTScreen_SetDeviceDoorState(uint8_t is_open)
{
  (void)is_open;
}
#endif

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
  (void)LT168B_WriteVersionText(LTSCREEN_CURRENT_VERSION_ADDR, current_version);
  (void)LT168B_WriteVersionText(LTSCREEN_LATEST_VERSION_ADDR, latest_version);
}

static void lt_screen_apply_device_door_icon(uint8_t is_open)
{
  if ((s_device_door_icon_applied != 0U) &&
      (s_device_door_open == ((is_open != 0U) ? 1U : 0U)))
  {
    return;
  }

  s_device_door_open = (is_open != 0U) ? 1U : 0U;
  s_device_door_icon_applied = 1U;

  if (s_device_door_open != 0U)
  {
    LT168B_WriteU16(LTSCREEN_DEVICE_DOOR_ADDR, 0x0000U);
  }
  else
  {
    LT168B_WriteU16(LTSCREEN_DEVICE_DOOR_ADDR, 0x0001U);
  }
}

static void lt_screen_handle_calibration_start(void)
{
  static const uint8_t text_done[] = {0xD2U, 0xD1U, 0xCDU, 0xEAU, 0xB3U, 0xC9U};

  s_lt_screen_calibration_done = 0U;
  osDelay(LTSCREEN_CALIBRATION_DELAY_MS);
  LT168B_SendStr(0x10U,
                 LTSCREEN_CALIBRATION_STATUS_ADDR,
                 text_done,
                 (uint8_t)sizeof(text_done));
  s_lt_screen_calibration_done = 1U;
}

static void lt_screen_reset_training_counts(void)
{
  uint8_t index;

  for (index = 0U; index < LTSCREEN_TRAINING_ACTION_COUNT; index++)
  {
    s_lt_screen_training_count[index] = 0U;
    s_lt_screen_training_last_written[index] = 0xFFU;
  }
}

static void lt_screen_write_training_count(uint16_t address, uint8_t count)
{
  uint8_t text[2];

  if (count > LTSCREEN_TRAINING_COUNT_MAX)
  {
    count = LTSCREEN_TRAINING_COUNT_MAX;
  }

  if (count < 10U)
  {
    text[0] = (uint8_t)('0' + count);
    text[1] = (uint8_t)' ';
  }
  else
  {
    text[0] = (uint8_t)('0' + (count / 10U));
    text[1] = (uint8_t)('0' + (count % 10U));
  }

  LT168B_SendStr(0x10U, address, text, (uint8_t)sizeof(text));
}

static uint16_t lt_screen_get_training_count_addr(uint8_t action_index)
{
  switch (action_index)
  {
    case 0U:
      return LTSCREEN_TRAINING_FRONT_RAISE_ADDR;
    case 1U:
      return LTSCREEN_TRAINING_SIDE_RAISE_ADDR;
    case 2U:
      return LTSCREEN_TRAINING_SHOULDER_RAISE_ADDR;
    case 3U:
      return LTSCREEN_TRAINING_ELBOW_FLEX_ADDR;
    default:
      return 0U;
  }
}

static void lt_screen_sync_training_count(uint8_t action_index)
{
  uint16_t address;

  if (action_index >= LTSCREEN_TRAINING_ACTION_COUNT)
  {
    return;
  }

  if (s_lt_screen_training_last_written[action_index] ==
      s_lt_screen_training_count[action_index])
  {
    return;
  }

  address = lt_screen_get_training_count_addr(action_index);
  if (address == 0U)
  {
    return;
  }

  lt_screen_write_training_count(address, s_lt_screen_training_count[action_index]);
  s_lt_screen_training_last_written[action_index] =
    s_lt_screen_training_count[action_index];
}

static void lt_screen_sync_training_counts(void)
{
  uint8_t index;

  for (index = 0U; index < LTSCREEN_TRAINING_ACTION_COUNT; index++)
  {
    lt_screen_sync_training_count(index);
  }
}

static int8_t lt_screen_find_training_action_index(int32_t action_label)
{
  switch ((motion_label_t)action_label)
  {
    case MOTION_LABEL_FRONT_RAISE:
      return 0;
    case MOTION_LABEL_SIDE_RAISE:
      return 1;
    case MOTION_LABEL_SHOULDER_RAISE:
      return 2;
    case MOTION_LABEL_ELBOW_FLEX:
      return 3;
    default:
      return -1;
  }
}

static void lt_screen_process_training_refresh(void)
{
  int32_t action_label;
  int8_t action_index;

  while ((s_lt_screen_training_active != 0U) &&
         (MotionEvents_TakeTrainingPageRefreshAction(&action_label) != 0U))
  {
    action_index = lt_screen_find_training_action_index(action_label);
    if (action_index < 0)
    {
      continue;
    }

    if (s_lt_screen_training_count[(uint8_t)action_index] < LTSCREEN_TRAINING_COUNT_MAX)
    {
      s_lt_screen_training_count[(uint8_t)action_index]++;
    }

    lt_screen_sync_training_count((uint8_t)action_index);
  }
}

static void lt_screen_handle_training_page_enter(void)
{
  if (s_lt_screen_calibration_done == 0U)
  {
    LT168B_DebugPrintLine("[LT SCREEN] training page blocked");
    return;
  }

  s_lt_screen_training_page_entered = 1U;
  s_lt_screen_training_active = 0U;
  LT168B_DebugPrintLine("[LT SCREEN] training page ready");
}

static void lt_screen_handle_training_start(void)
{
  if ((s_lt_screen_calibration_done == 0U) ||
      (s_lt_screen_training_page_entered == 0U))
  {
    LT168B_DebugPrintLine("[LT SCREEN] training blocked");
    return;
  }

  MotionEvents_ClearTrainingPageRefresh();
  lt_screen_reset_training_counts();
  lt_screen_sync_training_counts();
  s_lt_screen_training_active = 1U;
  Motion_RequestStart();
  LT168B_DebugPrintLine("[LT SCREEN] training start");
}

static void lt_screen_handle_training_stop(void)
{
  s_lt_screen_training_active = 0U;
  s_lt_screen_training_page_entered = 0U;
  lt_screen_reset_training_counts();
  lt_screen_sync_training_counts();
  MotionEvents_ClearTrainingPageRefresh();
  Motion_RequestStop();
  LT168B_DebugPrintLine("[LT SCREEN] training stop");
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
