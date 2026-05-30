#include "health_monitor.h"

#include "uart_screen.h"

#include <stdio.h>

#define HEALTH_MONITOR_COMP_DATE          "txt_time"
#define HEALTH_MONITOR_COMP_TIME          "txt_time"
#define HEALTH_MONITOR_COMP_WEATHER_ICON  "x0"
#define HEALTH_MONITOR_COMP_WEATHER_TEXT  "txt_temp"
#define HEALTH_MONITOR_COMP_OUTDOOR_TEMP  "txt_temp"
#define HEALTH_MONITOR_COMP_INDOOR_TEMP   "txt_temp"
#define HEALTH_MONITOR_COMP_HUMIDITY      "txt_hum"
#define HEALTH_MONITOR_COMP_TEMP_PROGRESS "j0"
#define HEALTH_MONITOR_COMP_HEART_ICON    "x2"
#define HEALTH_MONITOR_COMP_HEART_RATE    "txt_rate"
#define HEALTH_MONITOR_COMP_HEART_UNIT    "txt_bpm"
#define HEALTH_MONITOR_COMP_TRAIN1        "elbow_flex"
#define HEALTH_MONITOR_COMP_TRAIN2        "front_raise"
#define HEALTH_MONITOR_COMP_TRAIN3        "shoulder_raise"
#define HEALTH_MONITOR_COMP_TRAIN4        "side_raise"

#define HEALTH_MONITOR_HEART_ICON_PIC_ID 7U

static volatile uint8_t g_health_monitor_current_page = HEALTH_MONITOR_PAGE_HOME;

static uint8_t HealthMonitor_IsWeatherValid(WeatherType_t type)
{
  return ((uint32_t)type <= (uint32_t)WEATHER_FOGGY) ? 1U : 0U;
}

static void HealthMonitor_UpdateHeartSpo2(uint8_t heart_rate, uint8_t spo2)
{
  char text[16];

  (void)snprintf(text, sizeof(text), "%u/%u",
                 (unsigned int)heart_rate,
                 (unsigned int)spo2);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HEART_RATE, text);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HEART_UNIT, "HR/SPO2");
}

static void HealthMonitor_UpdateTrainingPlan(int32_t elbow_flex_count,
                                             int32_t front_raise_count,
                                             int32_t shoulder_raise_count,
                                             int32_t side_raise_count)
{
  char text[16];

  (void)snprintf(text, sizeof(text), "%ld", (long)elbow_flex_count);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TRAIN1, text);

  (void)snprintf(text, sizeof(text), "%ld", (long)front_raise_count);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TRAIN2, text);

  (void)snprintf(text, sizeof(text), "%ld", (long)shoulder_raise_count);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TRAIN3, text);

  (void)snprintf(text, sizeof(text), "%ld", (long)side_raise_count);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TRAIN4, text);
}

void HealthMonitor_Init(void)
{
  /* Reserved for future lightweight state init. Keep runtime page control in StartDefaultTask(). */
}

void HealthMonitor_SetPage(uint8_t page_id)
{
  g_health_monitor_current_page = page_id;
  Screen_Nextion_SetPage(page_id);
}

uint8_t HealthMonitor_GetCurrentPage(void)
{
  return g_health_monitor_current_page;
}

uint8_t HealthMonitor_IsTrainingPageActive(void)
{
  return (g_health_monitor_current_page == HEALTH_MONITOR_PAGE_TRAINING) ? 1U : 0U;
}

void HealthMonitor_UpdateDateTime(uint16_t year,
                                  uint8_t month,
                                  uint8_t day,
                                  uint8_t hour,
                                  uint8_t minute,
                                  uint8_t second)
{
  char text[24];

  (void)year;
  (void)month;
  (void)day;

  (void)snprintf(text, sizeof(text), "%02u:%02u",
                 (unsigned int)hour,
                 (unsigned int)minute);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TIME, text);
}

void HealthMonitor_SetWeather(WeatherType_t type, int8_t outdoor_temp)
{
  if (HealthMonitor_IsWeatherValid(type) != 0U)
  {
    Screen_Nextion_SetPicture(HEALTH_MONITOR_COMP_WEATHER_ICON, (uint16_t)type);
  }
  (void)outdoor_temp;
}

void HealthMonitor_UpdateTempHumi(int16_t indoor_temp_tenths, uint8_t humidity)
{
  char text[24];
  int32_t temp_abs = indoor_temp_tenths;
  char sign = '+';
  int32_t progress = indoor_temp_tenths / 10;

  if (temp_abs < 0)
  {
    sign = '-';
    temp_abs = -temp_abs;
  }

  if (progress < 0)
  {
    progress = 0;
  }
  else if (progress > 100)
  {
    progress = 100;
  }

  (void)snprintf(text, sizeof(text), "%c%ld.%ldC",
                 sign,
                 (long)(temp_abs / 10),
                 (long)(temp_abs % 10));
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_INDOOR_TEMP, text);

  (void)snprintf(text, sizeof(text), "%u%%", (unsigned int)humidity);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HUMIDITY, text);

  Screen_Nextion_SetValue(HEALTH_MONITOR_COMP_TEMP_PROGRESS, progress);
}

void HealthMonitor_SetHeartRate(uint8_t bpm)
{
  char text[8];

  (void)snprintf(text, sizeof(text), "%u", (unsigned int)bpm);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HEART_RATE, text);
}

void HealthMonitor_UpdateTrainingPlanOnly(const HealthData_t *data)
{
  if (data == NULL)
  {
    return;
  }

  HealthMonitor_UpdateTrainingPlan(data->elbow_flex_count,
                                   data->front_raise_count,
                                   data->shoulder_raise_count,
                                   data->side_raise_count);
}

void HealthMonitor_RefreshTrainingPlanOnly(void)
{
  Screen_Nextion_RefreshComponent(HEALTH_MONITOR_COMP_TRAIN1);
  Screen_Nextion_RefreshComponent(HEALTH_MONITOR_COMP_TRAIN2);
  Screen_Nextion_RefreshComponent(HEALTH_MONITOR_COMP_TRAIN3);
  Screen_Nextion_RefreshComponent(HEALTH_MONITOR_COMP_TRAIN4);
}

void HealthMonitor_UpdateAll(const HealthData_t *data)
{
  if (data == NULL)
  {
    return;
  }

  HealthMonitor_UpdateDateTime(data->year,
                               data->month,
                               data->day,
                               data->hour,
                               data->minute,
                               data->second);
  HealthMonitor_SetWeather(data->weather_type, data->outdoor_temp);
  HealthMonitor_UpdateTempHumi(data->indoor_temp_tenths, data->humidity);
  Screen_Nextion_SetPicture(HEALTH_MONITOR_COMP_HEART_ICON, HEALTH_MONITOR_HEART_ICON_PIC_ID);
  HealthMonitor_UpdateHeartSpo2(data->heart_rate, data->spo2);
  HealthMonitor_UpdateTrainingPlanOnly(data);
}

void HealthMonitor_SendDemoFrame(const HealthData_t *data)
{
  if ((Screen_IsReady() == 0U) || (data == NULL))
  {
    return;
  }

  HealthMonitor_UpdateAll(data);
}
