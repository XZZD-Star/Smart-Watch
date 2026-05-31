#ifndef __HEALTH_MONITOR_H
#define __HEALTH_MONITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  WEATHER_SUNNY = 0U,
  WEATHER_CLOUDY,
  WEATHER_RAINY,
  WEATHER_SNOWY,
  WEATHER_THUNDER,
  WEATHER_FOGGY
} WeatherType_t;

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  WeatherType_t weather_type;
  int8_t outdoor_temp;
  int16_t indoor_temp_tenths;
  uint8_t humidity;
  uint8_t heart_rate;
  uint8_t spo2;
  int32_t elbow_flex_count;
  int32_t front_raise_count;
  int32_t shoulder_raise_count;
  int32_t side_raise_count;
} HealthData_t;

#define HEALTH_MONITOR_PAGE_HOME     0U
#define HEALTH_MONITOR_PAGE_TRAINING 2U

void HealthMonitor_Init(void);
void HealthMonitor_SetPage(uint8_t page_id);
uint8_t HealthMonitor_GetCurrentPage(void);
uint8_t HealthMonitor_IsTrainingPageActive(void);
void HealthMonitor_UpdateDateTime(uint16_t year,
                                  uint8_t month,
                                  uint8_t day,
                                  uint8_t hour,
                                  uint8_t minute,
                                  uint8_t second);
void HealthMonitor_SetWeather(WeatherType_t type, int8_t outdoor_temp);
void HealthMonitor_UpdateTempHumi(int16_t indoor_temp_tenths, uint8_t humidity);
void HealthMonitor_SetHeartRate(uint8_t bpm);
void HealthMonitor_UpdateTrainingPlanOnly(const HealthData_t *data);
void HealthMonitor_RefreshTrainingPlanOnly(void);
void HealthMonitor_UpdateAll(const HealthData_t *data);
void HealthMonitor_SendDemoFrame(const HealthData_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __HEALTH_MONITOR_H */
