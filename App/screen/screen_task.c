#include "screen_task.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cmsis_os2.h"
#include "health_monitor.h"
#include "onenet.h"
#include "uart7_role.h"
#include "uart_screen.h"
#include "usart.h"

#define SCREEN_BOOT_READY_DELAY_MS   1200U
#define SCREEN_PAGE_SETTLE_DELAY_MS  50U
#define SCREEN_CLOCK_START_HOUR      20U
#define SCREEN_CLOCK_START_MINUTE    26U
#define SCREEN_CLOCK_START_SECOND    0U
#define SCREEN_FIXED_OUTDOOR_TEMP    26
#define SCREEN_FIXED_INDOOR_TEMP_TENTHS 265
#define SCREEN_FIXED_HUMIDITY        60U
#define SCREEN_TASK_POLL_INTERVAL_MS 200U
#define SCREEN_CLOCK_UPDATE_INTERVAL_MS 5000U
#define SCREEN_PAGE_QUERY_INTERVAL_MS 500U

static void screen_build_health_data(HealthData_t *data, uint32_t time_base_tick);
static void screen_track_current_page(uint32_t now_tick,
                                      uint32_t *last_page_query_tick,
                                      uint8_t *last_page,
                                      uint8_t *home_dirty,
                                      uint8_t *training_dirty);
#if APP_UART7_IS_SCREEN && APP_SCREEN_IS_COMPONENT_PROBE
static void screen_component_probe_init(void);
static void screen_component_probe_tick(void);
#endif

void ScreenTask_Run(void)
{
  HealthData_t screen_data;
  uint32_t screen_time_base_tick = 0U;
  uint32_t last_clock_update_tick = 0U;
  uint32_t last_page_query_tick = 0U;
  uint8_t last_screen_page = HEALTH_MONITOR_PAGE_HOME;
  uint8_t home_dirty = 1U;
  uint8_t training_dirty = 1U;

  /* defaultTask 只维护低频屏幕刷新，不参与运动识别和网络发送。 */
#if APP_UART7_IS_SCREEN
  Screen_Init(&huart7, SCREEN_TYPE_NEXTION, SCREEN_BAUD_115200);
  /* 首次切页前等待串口屏完成上电初始化。 */
  osDelay(SCREEN_BOOT_READY_DELAY_MS);
#if APP_SCREEN_IS_HEALTH_MONITOR
  HealthMonitor_Init();
  screen_time_base_tick = osKernelGetTickCount();
  screen_build_health_data(&screen_data, screen_time_base_tick);
  HealthMonitor_SetPage(HEALTH_MONITOR_PAGE_HOME);
  osDelay(SCREEN_PAGE_SETTLE_DELAY_MS);
  HealthMonitor_SendDemoFrame(&screen_data);
  home_dirty = 0U;
  last_screen_page = HealthMonitor_GetCurrentPage();
  last_clock_update_tick = osKernelGetTickCount();
  last_page_query_tick = last_clock_update_tick;
  Screen_Nextion_RequestPageId();
#else
  screen_component_probe_init();
#endif
#endif

  /* 周期性刷新屏幕数据。 */
  for(;;)
  {
#if APP_UART7_IS_SCREEN
#if APP_SCREEN_IS_HEALTH_MONITOR
    uint32_t now_tick = osKernelGetTickCount();
    uint8_t current_page = HEALTH_MONITOR_PAGE_HOME;

    screen_track_current_page(now_tick,
                              &last_page_query_tick,
                              &last_screen_page,
                              &home_dirty,
                              &training_dirty);

    current_page = HealthMonitor_GetCurrentPage();

    if (current_page != HEALTH_MONITOR_PAGE_HOME)
    {
      home_dirty = 1U;
    }
    if (current_page != HEALTH_MONITOR_PAGE_TRAINING)
    {
      training_dirty = 1U;
    }

    if (current_page == HEALTH_MONITOR_PAGE_HOME)
    {
      if (home_dirty != 0U)
      {
        screen_build_health_data(&screen_data, screen_time_base_tick);
        HealthMonitor_UpdateHomeOnly(&screen_data);
        home_dirty = 0U;
        last_clock_update_tick = now_tick;
      }
      else if ((now_tick - last_clock_update_tick) >= SCREEN_CLOCK_UPDATE_INTERVAL_MS)
      {
        screen_build_health_data(&screen_data, screen_time_base_tick);
        HealthMonitor_UpdateDateTime(screen_data.year,
                                     screen_data.month,
                                     screen_data.day,
                                     screen_data.hour,
                                     screen_data.minute,
                                     screen_data.second);
        last_clock_update_tick = now_tick;
      }
    }
    else if (current_page == HEALTH_MONITOR_PAGE_TRAINING)
    {
      if (training_dirty != 0U)
      {
        screen_build_health_data(&screen_data, screen_time_base_tick);
        HealthMonitor_UpdateTrainingPlanOnly(&screen_data);
        HealthMonitor_RefreshTrainingPlanOnly();
        training_dirty = 0U;
      }
    }
#else
    screen_component_probe_tick();
#endif
#endif
    osDelay(SCREEN_TASK_POLL_INTERVAL_MS);
  }
}

static void screen_build_health_data(HealthData_t *data, uint32_t time_base_tick)
{
  OneNetTrainPlanSnapshot_t display_snapshot;
  uint32_t now_tick;
  uint32_t elapsed_seconds;
  uint32_t total_seconds;

  if (data == NULL)
  {
    return;
  }

  memset(data, 0, sizeof(*data));

  now_tick = osKernelGetTickCount();
  elapsed_seconds = (now_tick - time_base_tick) / 1000U;
  total_seconds = (SCREEN_CLOCK_START_HOUR * 3600U) +
                  (SCREEN_CLOCK_START_MINUTE * 60U) +
                  SCREEN_CLOCK_START_SECOND +
                  elapsed_seconds;
  total_seconds %= (24U * 3600U);

  /* 屏幕只读取训练计划快照，不关心 MQTT 属性解析细节。 */
  OneNet_GetTrainDisplaySnapshot(&display_snapshot);

  data->hour = (uint8_t)(total_seconds / 3600U);
  data->minute = (uint8_t)((total_seconds % 3600U) / 60U);
  data->second = (uint8_t)(total_seconds % 60U);
  data->weather_type = WEATHER_SUNNY;
  data->outdoor_temp = SCREEN_FIXED_OUTDOOR_TEMP;
  data->indoor_temp_tenths = SCREEN_FIXED_INDOOR_TEMP_TENTHS;
  data->humidity = SCREEN_FIXED_HUMIDITY;
  data->heart_rate = ONENET_FIXED_HEART_RATE;
  data->spo2 = ONENET_FIXED_SPO2;
  data->elbow_flex_count = display_snapshot.elbow_flex_count;
  data->front_raise_count = display_snapshot.front_raise_count;
  data->shoulder_raise_count = display_snapshot.shoulder_raise_count;
  data->side_raise_count = display_snapshot.side_raise_count;
}

static void screen_track_current_page(uint32_t now_tick,
                                      uint32_t *last_page_query_tick,
                                      uint8_t *last_page,
                                      uint8_t *home_dirty,
                                      uint8_t *training_dirty)
{
  uint8_t page_id = 0U;

  if ((last_page_query_tick == NULL) ||
      (last_page == NULL) ||
      (home_dirty == NULL) ||
      (training_dirty == NULL))
  {
    return;
  }

  if (Screen_Nextion_TakeLatestPageId(&page_id) != 0U)
  {
    if (page_id != *last_page)
    {
      if (page_id == HEALTH_MONITOR_PAGE_HOME)
      {
        *home_dirty = 1U;
      }
      else if (page_id == HEALTH_MONITOR_PAGE_TRAINING)
      {
        *training_dirty = 1U;
      }

      *last_page = page_id;
    }

    HealthMonitor_SetCurrentPage(page_id);
  }

  if ((now_tick - *last_page_query_tick) >= SCREEN_PAGE_QUERY_INTERVAL_MS)
  {
    Screen_Nextion_RequestPageId();
    *last_page_query_tick = now_tick;
  }
}

/* 组件探测模式仅在显式配置时编译，用于定位屏幕控件名。 */
#if APP_UART7_IS_SCREEN && APP_SCREEN_IS_COMPONENT_PROBE
static void screen_component_probe_init(void)
{
  Screen_Nextion_SetPage(0U);
  osDelay(SCREEN_PAGE_SETTLE_DELAY_MS);

  Screen_Nextion_SetText("txt_time", "TIME_INIT");
  Screen_Nextion_SetText("txt_temp", "TEMP_INIT");
  Screen_Nextion_SetText("txt_hum", "HUM_INIT");
  Screen_Nextion_SetText("txt_rate", "66");
  Screen_Nextion_SetText("txt_bpm", "BPM");
  Screen_Nextion_SetValue("j0", 25);
  Screen_Nextion_SetPicture("x0", 0U);
  Screen_Nextion_SetPicture("x2", 7U);
}

static void screen_component_probe_tick(void)
{
  static uint32_t counter = 0U;
  char text[24];

  Screen_Nextion_SetPage(0U);
  osDelay(SCREEN_PAGE_SETTLE_DELAY_MS);

  (void)snprintf(text, sizeof(text), "TIME_%lu", (unsigned long)(counter % 1000U));
  Screen_Nextion_SetText("txt_time", text);

  (void)snprintf(text, sizeof(text), "TEMP_%lu", (unsigned long)((counter + 1U) % 1000U));
  Screen_Nextion_SetText("txt_temp", text);

  (void)snprintf(text, sizeof(text), "HUM_%lu", (unsigned long)((counter + 2U) % 1000U));
  Screen_Nextion_SetText("txt_hum", text);

  (void)snprintf(text, sizeof(text), "%lu", (unsigned long)(60U + (counter % 30U)));
  Screen_Nextion_SetText("txt_rate", text);
  Screen_Nextion_SetText("txt_bpm", "BPM");

  Screen_Nextion_SetValue("j0", (int32_t)(counter % 100U));
  Screen_Nextion_SetPicture("x0", (uint16_t)(counter % 6U));
  Screen_Nextion_SetPicture("x2", 7U);

  counter++;
}
#endif
