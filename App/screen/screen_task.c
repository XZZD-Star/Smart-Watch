#include "screen_task.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cmsis_os2.h"
#include "health_monitor.h"
#include "motion_app_events.h"
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
#define SCREEN_TRAINING_REFRESH_PASSES 2U

static void screen_build_health_data(HealthData_t *data, uint32_t time_base_tick);
#if APP_UART7_IS_SCREEN && APP_SCREEN_IS_COMPONENT_PROBE
static void screen_component_probe_init(void);
static void screen_component_probe_tick(void);
#endif

void ScreenTask_Run(void)
{
  HealthData_t screen_data;
  uint32_t screen_time_base_tick = 0U;

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
#else
  screen_component_probe_init();
#endif
#endif

  /* 周期性刷新屏幕数据。 */
  for(;;)
  {
#if APP_UART7_IS_SCREEN
#if APP_SCREEN_IS_HEALTH_MONITOR
    uint8_t refresh_training_page = 0U;
    uint8_t refresh_pass = 0U;

    refresh_training_page = MotionEvents_TakeTrainingPageRefresh();

    screen_build_health_data(&screen_data, screen_time_base_tick);

    if (refresh_training_page != 0U)
    {
      for (refresh_pass = 0U;
           refresh_pass < SCREEN_TRAINING_REFRESH_PASSES;
           refresh_pass++)
      {
        HealthMonitor_SetPage(HEALTH_MONITOR_PAGE_HOME);
        osDelay(SCREEN_PAGE_SETTLE_DELAY_MS);
        HealthMonitor_SetPage(HEALTH_MONITOR_PAGE_TRAINING);
        osDelay(SCREEN_PAGE_SETTLE_DELAY_MS);
        /* 切页后重新取快照，保证强制刷新使用最新训练次数。 */
        screen_build_health_data(&screen_data, screen_time_base_tick);
        HealthMonitor_UpdateTrainingPlanOnly(&screen_data);
        HealthMonitor_RefreshTrainingPlanOnly();
      }
    }

    HealthMonitor_UpdateAll(&screen_data);
#else
    screen_component_probe_tick();
#endif
#endif
    osDelay(1000);
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
