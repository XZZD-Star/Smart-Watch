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

typedef enum
{
  HEALTH_MONITOR_TRAIN_ITEM_ELBOW_FLEX = 0U,
  HEALTH_MONITOR_TRAIN_ITEM_FRONT_RAISE,
  HEALTH_MONITOR_TRAIN_ITEM_SHOULDER_RAISE,
  HEALTH_MONITOR_TRAIN_ITEM_SIDE_RAISE,
  HEALTH_MONITOR_TRAIN_ITEM_INVALID
} HealthMonitorTrainItem_t;

/* 屏幕任务调用：初始化健康页状态。 */
void HealthMonitor_Init(void);
/* 切换健康屏页面。 */
void HealthMonitor_SetPage(uint8_t page_id);
/* 记录屏幕当前页面，不向屏幕发送切页命令。 */
void HealthMonitor_SetCurrentPage(uint8_t page_id);
/* 读取当前记录的健康屏页面号。 */
uint8_t HealthMonitor_GetCurrentPage(void);
/* 判断当前是否处于训练计划页面。 */
uint8_t HealthMonitor_IsTrainingPageActive(void);
/* 更新时间显示控件。 */
void HealthMonitor_UpdateDateTime(uint16_t year,
                                  uint8_t month,
                                  uint8_t day,
                                  uint8_t hour,
                                  uint8_t minute,
                                  uint8_t second);
/* 更新天气图标和室外温度。 */
void HealthMonitor_SetWeather(WeatherType_t type, int8_t outdoor_temp);
/* 更新室内温度和湿度控件。 */
void HealthMonitor_UpdateTempHumi(int16_t indoor_temp_tenths, uint8_t humidity);
/* 更新心率控件。 */
void HealthMonitor_SetHeartRate(uint8_t bpm);
/* 只更新训练计划控件，动作完成后局部刷新使用。 */
void HealthMonitor_UpdateTrainingPlanOnly(const HealthData_t *data);
void HealthMonitor_UpdateTrainingPlanItem(HealthMonitorTrainItem_t item, int32_t count);
/* 强制刷新训练计划控件，处理 Nextion 页面切换后的显示残留。 */
void HealthMonitor_RefreshTrainingPlanOnly(void);
void HealthMonitor_RefreshTrainingPlanItem(HealthMonitorTrainItem_t item);
void HealthMonitor_UpdateHomeOnly(const HealthData_t *data);
/* 更新健康页全部控件。 */
void HealthMonitor_UpdateAll(const HealthData_t *data);
/* 屏幕启动后发送第一帧演示数据。 */
void HealthMonitor_SendDemoFrame(const HealthData_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __HEALTH_MONITOR_H */
