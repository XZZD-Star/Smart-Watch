#include "cloud_task.h"

#include <stdint.h>
#include <stddef.h>

#include "cmsis_os2.h"
#include "debug_uart7.h"
#include "ESP8266.h"
#include "motion_app_events.h"
#include "onenet.h"

void CloudTask_Run(void)
{
  uint32_t last_publish_tick = 0U;
  uint8_t subscribe_logged = 0U;

  /* Task2 只负责网络会话和上报，优先级低于运动识别任务。 */
  osDelay(1000);
  Debug_Printf("NET TASK START\r\n");

  for(;;)
  {
    ESP8266_Clear();
    ESP8266_ClearTransportError();
    OneNet_ClearSessionError();
    OneNet_ResetSubscribeState();
    subscribe_logged = 0U;

    while (ESP8266_Init() == 0U)
    {
      Debug_Printf("[TCP] INIT FAIL code=%u\r\n", (unsigned int)ESP8266_GetLastInitStatus());
      ESP8266_Clear();
      ESP8266_ClearTransportError();
      osDelay(1000);
    }
    Debug_Printf("[TCP] INIT OK\r\n");

    while (OneNet_DevLink() == 0U)
    {
      Debug_Printf("[MQTT] CONNECT FAIL code=%u ack=%u\r\n",
                   (unsigned int)OneNet_GetLastStatus(),
                   (unsigned int)OneNet_GetLastConnAckCode());
      if (ESP8266_HasTransportError() != 0U)
      {
        Debug_Printf("[TCP] TRANSPORT ERROR\r\n");
        break;
      }
      osDelay(1000);
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      Debug_Printf("[TCP] RESTART FROM INIT\r\n");
      osDelay(1000);
      continue;
    }
    Debug_Printf("[MQTT] CONNECT OK\r\n");

    if (OneNet_Subscribe() == 0U)
    {
      Debug_Printf("[MQTT] SUBSCRIBE SEND FAIL code=%u topic=%s\r\n",
                   (unsigned int)OneNet_GetLastStatus(),
                   OneNet_GetLastSubscribeTopic());
      ESP8266_Clear();
      ESP8266_ClearTransportError();
      OneNet_ClearSessionError();
      Debug_Printf("[TCP] WIFI REUSE IF CONNECTED, REINIT TCP/MQTT\r\n");
      osDelay(1000);
      continue;
    }
    Debug_Printf("[MQTT] SUBSCRIBE SENT\r\n");

    osDelay(200);
    last_publish_tick = osKernelGetTickCount();

    for (;;)
    {
      float pending_test_confidence = 0.0f;
      int32_t pending_test_value = 0;
      uint8_t *ipd_data = NULL;
      int32_t pending_action_kind_value = 0;
      uint32_t now = osKernelGetTickCount();
      uint8_t has_pending_test_conf = 0U;
      uint8_t has_pending_action_kind = 0U;
      if ((now - last_publish_tick) >= ONENET_PUBLISH_INTERVAL_MS)
      {
        if (OneNet_Publish() == 0U)
        {
          Debug_Printf("[MQTT] PUBLISH FAIL code=%u\r\n", (unsigned int)OneNet_GetLastStatus());
          break;
        }
        last_publish_tick = now;
      }

      /* 从运动任务队列取待上报结果，避免 Task1 直接阻塞在 MQTT 发送上。 */
      has_pending_test_conf = MotionEvents_PeekTestConfidence(&pending_test_value, &pending_test_confidence);
      has_pending_action_kind = MotionEvents_PeekActionKind(&pending_action_kind_value);

      if ((subscribe_logged != 0U) && (has_pending_test_conf != 0U))
      {
        if (OneNet_PostTestConfidenceValue(pending_test_value, pending_test_confidence) == 0U)
        {
          Debug_Printf("[MQTT][ERR] test+confidence post failed test=%ld conf=%.3f code=%u\r\n",
                       (long)pending_test_value,
                       (double)pending_test_confidence,
                       (unsigned int)OneNet_GetLastStatus());
          break;
        }

        MotionEvents_ClearTestConfidenceIfCurrent(pending_test_value);

        Debug_Printf("[MQTT] test+confidence posted on net task test=%ld conf=%.3f\r\n",
                     (long)pending_test_value,
                     (double)pending_test_confidence);
      }

      if ((subscribe_logged != 0U) && (has_pending_action_kind != 0U))
      {
        if (OneNet_PostActionKindValue(pending_action_kind_value) == 0U)
        {
          Debug_Printf("[MQTT][ERR] action_kind post failed value=%ld code=%u\r\n",
                       (long)pending_action_kind_value,
                       (unsigned int)OneNet_GetLastStatus());
          break;
        }

        MotionEvents_ClearActionKindIfCurrent(pending_action_kind_value);

        Debug_Printf("[MQTT] action_kind posted on net task value=%ld\r\n",
                     (long)pending_action_kind_value);
      }

      if (subscribe_logged != 0U)
      {
        if (OneNet_PostPendingFallAlarm() == 0U)
        {
          Debug_Printf("[MQTT][ERR] fall alarm post failed code=%u\r\n",
                       (unsigned int)OneNet_GetLastStatus());
          break;
        }
      }

      if (subscribe_logged != 0U)
      {
        if (OneNet_PostPendingDoorState() == 0U)
        {
          Debug_Printf("[MQTT][ERR] door state post failed code=%u\r\n",
                       (unsigned int)OneNet_GetLastStatus());
          break;
        }
      }

      if (subscribe_logged != 0U)
      {
        if (OneNet_PostPendingTrainPlan() == 0U)
        {
          Debug_Printf("[MQTT][ERR] train plan post failed code=%u\r\n",
                       (unsigned int)OneNet_GetLastStatus());
          break;
        }
      }

      /* 下行 MQTT payload 统一交给 OneNet 协议层解析。 */
      ipd_data = ESP8266_GetIPD(50U);
      if (ipd_data != NULL)
      {
        OneNet_RevPro(ipd_data);
        ESP8266_Clear();
      }

      if ((subscribe_logged == 0U) && (OneNet_IsSubscribeReady() != 0U))
      {
        Debug_Printf("[MQTT] SUBSCRIBE OK\r\n");
        subscribe_logged = 1U;
        last_publish_tick = now;
      }

      if ((ESP8266_HasTransportError() != 0U) || (OneNet_HasSessionError() != 0U))
      {
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
        break;
      }

      if (subscribe_logged == 0U)
      {
        osDelay(10);
        continue;
      }

      osDelay(10);
    }

    ESP8266_Clear();
    ESP8266_ClearTransportError();
    OneNet_ClearSessionError();
    Debug_Printf("[TCP] WIFI REUSE IF CONNECTED, REINIT TCP/MQTT\r\n");
    osDelay(1000);
  }
}
