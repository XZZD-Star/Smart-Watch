#include "cloud_task.h"

#include <stdint.h>
#include <stddef.h>

#include "cmsis_os2.h"
#include "debug_uart7.h"
#include "ESP8266.h"
#include "motion_app_events.h"
#include "onenet.h"

static void cloud_reset_session(void);
static void cloud_wait_wifi_init(void);
static uint8_t cloud_connect_mqtt(void);
static uint8_t cloud_send_subscribe(void);
static void cloud_restart_session_delay(void);
static void cloud_service_loop(void);
static uint8_t cloud_publish_periodic(uint32_t now, uint32_t *last_publish_tick);
static uint8_t cloud_post_motion_events(uint8_t subscribe_logged);
static uint8_t cloud_post_onenet_states(uint8_t subscribe_logged);
static void cloud_process_downlink(
  uint8_t *subscribe_logged,
  uint32_t *last_publish_tick,
  uint32_t now);
static uint8_t cloud_has_loop_error(void);

void CloudTask_Run(void)
{
  /* Task2 只负责网络会话和上报，优先级低于运动识别任务。 */
  osDelay(1000);
  Debug_Printf("NET TASK START\r\n");

  for(;;)
  {
    cloud_reset_session();
    cloud_wait_wifi_init();

    if (cloud_connect_mqtt() == 0U)
    {
      Debug_Printf("[TCP] RESTART FROM INIT\r\n");
      osDelay(1000);
      continue;
    }

    if (cloud_send_subscribe() == 0U)
    {
      cloud_restart_session_delay();
      continue;
    }

    cloud_service_loop();
    cloud_restart_session_delay();
  }
}

static void cloud_reset_session(void)
{
  ESP8266_Clear();
  ESP8266_ClearTransportError();
  OneNet_ClearSessionError();
  OneNet_ResetSubscribeState();
}

static void cloud_wait_wifi_init(void)
{
  while (ESP8266_Init() == 0U)
  {
    Debug_Printf("[TCP] INIT FAIL code=%u\r\n", (unsigned int)ESP8266_GetLastInitStatus());
    ESP8266_Clear();
    ESP8266_ClearTransportError();
    osDelay(1000);
  }

  Debug_Printf("[TCP] INIT OK\r\n");
}

static uint8_t cloud_connect_mqtt(void)
{
  while (OneNet_DevLink() == 0U)
  {
    Debug_Printf("[MQTT] CONNECT FAIL code=%u ack=%u\r\n",
                 (unsigned int)OneNet_GetLastStatus(),
                 (unsigned int)OneNet_GetLastConnAckCode());

    if (ESP8266_HasTransportError() != 0U)
    {
      Debug_Printf("[TCP] TRANSPORT ERROR\r\n");
      return 0U;
    }

    osDelay(1000);
  }

  if (ESP8266_HasTransportError() != 0U)
  {
    return 0U;
  }

  Debug_Printf("[MQTT] CONNECT OK\r\n");
  return 1U;
}

static uint8_t cloud_send_subscribe(void)
{
  if (OneNet_Subscribe() == 0U)
  {
    Debug_Printf("[MQTT] SUBSCRIBE SEND FAIL code=%u topic=%s\r\n",
                 (unsigned int)OneNet_GetLastStatus(),
                 OneNet_GetLastSubscribeTopic());
    return 0U;
  }

  Debug_Printf("[MQTT] SUBSCRIBE SENT\r\n");
  return 1U;
}

static void cloud_restart_session_delay(void)
{
  ESP8266_Clear();
  ESP8266_ClearTransportError();
  OneNet_ClearSessionError();
  Debug_Printf("[TCP] WIFI REUSE IF CONNECTED, REINIT TCP/MQTT\r\n");
  osDelay(1000);
}

static void cloud_service_loop(void)
{
  uint32_t last_publish_tick;
  uint8_t subscribe_logged = 0U;

  osDelay(200);
  last_publish_tick = osKernelGetTickCount();

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();

    if (cloud_post_motion_events(subscribe_logged) == 0U)
    {
      break;
    }

    if (cloud_post_onenet_states(subscribe_logged) == 0U)
    {
      break;
    }

    if (cloud_publish_periodic(now, &last_publish_tick) == 0U)
    {
      break;
    }

    cloud_process_downlink(&subscribe_logged, &last_publish_tick, now);

    if (cloud_has_loop_error() != 0U)
    {
      break;
    }

    osDelay(10);
  }
}

static uint8_t cloud_publish_periodic(uint32_t now, uint32_t *last_publish_tick)
{
  if ((now - *last_publish_tick) < ONENET_PUBLISH_INTERVAL_MS)
  {
    return 1U;
  }

  if (OneNet_Publish() == 0U)
  {
    Debug_Printf("[MQTT] PUBLISH FAIL code=%u\r\n", (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  *last_publish_tick = now;
  return 1U;
}

static uint8_t cloud_post_motion_events(uint8_t subscribe_logged)
{
  float pending_test_confidence = 0.0f;
  int32_t pending_test_value = 0;
  int32_t pending_action_kind_value = 0;
  uint8_t has_pending_test_conf = 0U;
  uint8_t has_pending_action_kind = 0U;

  has_pending_test_conf =
    MotionEvents_PeekTestConfidence(&pending_test_value, &pending_test_confidence);
  has_pending_action_kind = MotionEvents_PeekActionKind(&pending_action_kind_value);

  if (subscribe_logged == 0U)
  {
    return 1U;
  }

  if (has_pending_test_conf != 0U)
  {
    if (OneNet_PostTestConfidenceValue(pending_test_value, pending_test_confidence) == 0U)
    {
      Debug_Printf("[MQTT][ERR] test+confidence post failed test=%ld conf=%.3f code=%u\r\n",
                   (long)pending_test_value,
                   (double)pending_test_confidence,
                   (unsigned int)OneNet_GetLastStatus());
      return 0U;
    }

    MotionEvents_ClearTestConfidenceIfCurrent(pending_test_value);
    Debug_Printf("[MQTT] test+confidence posted on net task test=%ld conf=%.3f\r\n",
                 (long)pending_test_value,
                 (double)pending_test_confidence);
  }

  if (has_pending_action_kind != 0U)
  {
    if (OneNet_PostActionKindValue(pending_action_kind_value) == 0U)
    {
      Debug_Printf("[MQTT][ERR] action_kind post failed value=%ld code=%u\r\n",
                   (long)pending_action_kind_value,
                   (unsigned int)OneNet_GetLastStatus());
      return 0U;
    }

    MotionEvents_ClearActionKindIfCurrent(pending_action_kind_value);
    Debug_Printf("[MQTT] action_kind posted on net task value=%ld\r\n",
                 (long)pending_action_kind_value);
  }

  return 1U;
}

static uint8_t cloud_post_onenet_states(uint8_t subscribe_logged)
{
  if (subscribe_logged == 0U)
  {
    return 1U;
  }

  if (OneNet_PostPendingFallAlarm() == 0U)
  {
    Debug_Printf("[MQTT][ERR] fall alarm post failed code=%u\r\n",
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  if (OneNet_PostPendingDoorState() == 0U)
  {
    Debug_Printf("[MQTT][ERR] door state post failed code=%u\r\n",
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  if (OneNet_PostPendingTrainPlan() == 0U)
  {
    Debug_Printf("[MQTT][ERR] train plan post failed code=%u\r\n",
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  return 1U;
}

static void cloud_process_downlink(
  uint8_t *subscribe_logged,
  uint32_t *last_publish_tick,
  uint32_t now)
{
  uint8_t *ipd_data = ESP8266_GetIPD(50U);

  if (ipd_data != NULL)
  {
    OneNet_RevPro(ipd_data);
    ESP8266_Clear();
  }

  if ((*subscribe_logged == 0U) && (OneNet_IsSubscribeReady() != 0U))
  {
    Debug_Printf("[MQTT] SUBSCRIBE OK\r\n");
    *subscribe_logged = 1U;
    *last_publish_tick = now;
  }
}

static uint8_t cloud_has_loop_error(void)
{
  if ((ESP8266_HasTransportError() == 0U) && (OneNet_HasSessionError() == 0U))
  {
    return 0U;
  }

  if (ESP8266_HasTransportError() != 0U)
  {
    Debug_Printf("[TCP] TRANSPORT ERROR IN LOOP\r\n");
  }

  if (OneNet_HasSessionError() != 0U)
  {
    if (OneNet_GetLastStatus() == ONENET_STATUS_FAIL_SUB_ACK)
    {
      Debug_Printf("[MQTT] SUBSCRIBE FAIL code=%u topic=%s\r\n",
                   (unsigned int)OneNet_GetLastStatus(),
                   OneNet_GetLastSubscribeTopic());
    }
    else
    {
      Debug_Printf("[MQTT] SESSION ERROR code=%u\r\n", (unsigned int)OneNet_GetLastStatus());
    }
  }

  return 1U;
}
