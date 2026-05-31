#include "onenet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ESP8266.h"
#include "MqttKit.h"
#include "debug_uart7.h"
#include "motion_ai.h"
#include "motion_input.h"
#include "tim.h"

#define ONENET_PACKET_TIMEOUT_MS       3000U
#define ONENET_DOWNLINK_TOPIC_SIZE      128U
#define ONENET_DOWNLINK_PAYLOAD_SIZE    256U
#define ONENET_REQUEST_ID_SIZE           48U
#define ONENET_REPLY_PAYLOAD_SIZE       128U
#define ONENET_PLAN_POST_PAYLOAD_SIZE   320U
#define ONENET_SUBSCRIBE_GROUP_NAME      ONENET_TOPIC_PROP_SET
#define ONENET_REPLY_CODE_OK            200U
#define ONENET_REPLY_CODE_BAD_REQUEST   400U
#define ONENET_TEST_ACTION_ID_MIN         1
#define ONENET_TEST_ACTION_ID_MAX         6
#define ONENET_TEST_SCORE_MIN             0
#define ONENET_TEST_SCORE_MAX             2

static volatile uint8_t g_onenet_session_error = 0U;
static volatile uint8_t g_onenet_last_status = ONENET_STATUS_OK;
static volatile uint8_t g_onenet_last_connack_code = 0U;
static volatile uint8_t g_onenet_subscribe_ready = 0U;
static volatile uint32_t g_onenet_downlink_count = 0U;
static volatile uint32_t g_onenet_publish_seq = 1U;
static volatile uint8_t g_onenet_has_valid_test_value = 0U;
static volatile uint8_t g_onenet_train_plan_post_pending = 0U;
static volatile uint8_t g_onenet_door_state_post_pending = 0U;
static volatile uint8_t g_onenet_fall_alarm_active = 0U;
static volatile uint8_t g_onenet_fall_alarm_post_pending = 0U;
static volatile int32_t g_onenet_last_valid_test_value = 0;
static volatile int32_t g_onenet_pending_door_state_value = 0;
static volatile int32_t g_onenet_pending_fall_alarm_value = 0;
static char g_onenet_last_topic[ONENET_DOWNLINK_TOPIC_SIZE];
static char g_onenet_last_payload[ONENET_DOWNLINK_PAYLOAD_SIZE];
static const char *g_onenet_last_subscribe_topic = "";

typedef struct
{
  int32_t elbow_flex_count;
  int32_t front_raise_count;
  int32_t shoulder_raise_count;
  int32_t side_raise_count;
} onenet_train_plan_t;

typedef struct
{
  char request_id[ONENET_REQUEST_ID_SIZE];
  int32_t test_value;
  int32_t decoded_action_id;
  int32_t decoded_score;
  int32_t open_value;
  int32_t start_value;
  int32_t fall_alarm_value;
  uint8_t has_request_id;
  uint8_t has_test_value;
  uint8_t test_value_valid;
  uint8_t has_plan_update;
  uint8_t has_open_value;
  uint8_t open_value_valid;
  uint8_t has_start_value;
  uint8_t start_value_valid;
  uint8_t has_fall_alarm_value;
  uint8_t fall_alarm_value_valid;
  uint16_t reply_code;
  const char *reply_message;
} onenet_prop_set_context_t;

static onenet_train_plan_t g_onenet_train_plan = {0};
static onenet_train_plan_t g_onenet_train_display = {0};

static void OneNet_ResetTrainDisplayFromPlan(void)
{
  g_onenet_train_display.elbow_flex_count = -g_onenet_train_plan.elbow_flex_count;
  g_onenet_train_display.front_raise_count = -g_onenet_train_plan.front_raise_count;
  g_onenet_train_display.shoulder_raise_count = -g_onenet_train_plan.shoulder_raise_count;
  g_onenet_train_display.side_raise_count = -g_onenet_train_plan.side_raise_count;
}

static int32_t *OneNet_GetTrainDisplaySlotByAction(int32_t action_label)
{
  switch ((motion_label_t)action_label)
  {
    case MOTION_LABEL_ELBOW_FLEX:
      return &g_onenet_train_display.elbow_flex_count;

    case MOTION_LABEL_FRONT_RAISE:
      return &g_onenet_train_display.front_raise_count;

    case MOTION_LABEL_SHOULDER_RAISE:
      return &g_onenet_train_display.shoulder_raise_count;

    case MOTION_LABEL_SIDE_RAISE:
      return &g_onenet_train_display.side_raise_count;

    default:
      return NULL;
  }
}

static uint8_t OneNet_PublishRaw(const char *topic,
                                 const char *payload,
                                 uint32_t payload_len,
                                 uint8_t packet_fail_status,
                                 uint8_t send_fail_status);

static uint8_t OneNet_PostIntPropertyValue(const char *property_key,
                                           int32_t value,
                                           const char *log_tag)
{
  char payload[ONENET_REPLY_PAYLOAD_SIZE + 48U];
  int payload_len = 0;

  if ((property_key == NULL) || (log_tag == NULL))
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%ld}}}",
                         (unsigned long)g_onenet_publish_seq,
                         property_key,
                         (long)value);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] PROP POST %s payload build fail value=%ld\r\n",
                 log_tag,
                 (long)value);
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] PROP POST %s TX topic=%s payload=%s\r\n",
               log_tag,
               ONENET_TOPIC_PROP_POST,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] PROP POST %s send fail value=%ld code=%u\r\n",
                 log_tag,
                 (long)value,
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  g_onenet_publish_seq++;
  Debug_Printf("[MQTT] PROP POST %s OK value=%ld\r\n",
               log_tag,
               (long)value);
  return 1U;
}

static uint8_t OneNet_PostTrainPlan(void)
{
  char payload[ONENET_PLAN_POST_PAYLOAD_SIZE];
  int payload_len = 0;

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%ld},\"%s\":{\"value\":%ld},\"%s\":{\"value\":%ld},\"%s\":{\"value\":%ld}}}",
                         (unsigned long)g_onenet_publish_seq,
                         ONENET_DP_ELBOW_FLEX_COUNT_KEY,
                         (long)g_onenet_train_plan.elbow_flex_count,
                         ONENET_DP_FRONT_RAISE_COUNT_KEY,
                         (long)g_onenet_train_plan.front_raise_count,
                         ONENET_DP_SHOULDER_RAISE_COUNT_KEY,
                         (long)g_onenet_train_plan.shoulder_raise_count,
                         ONENET_DP_SIDE_RAISE_COUNT_KEY,
                         (long)g_onenet_train_plan.side_raise_count);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] PLAN POST payload build fail\r\n");
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] PLAN POST TX topic=%s payload=%s\r\n",
               ONENET_TOPIC_PROP_POST,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] PLAN POST send fail code=%u\r\n",
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  g_onenet_publish_seq++;
  Debug_Printf("[MQTT] PLAN POST OK elbow=%ld front=%ld shoulder=%ld side=%ld\r\n",
               (long)g_onenet_train_plan.elbow_flex_count,
               (long)g_onenet_train_plan.front_raise_count,
               (long)g_onenet_train_plan.shoulder_raise_count,
               (long)g_onenet_train_plan.side_raise_count);
  return 1U;
}

static uint8_t OneNet_TopicMatches(const char *topic, uint16_t topic_len, const char *expected)
{
  size_t expected_len = 0U;

  if ((topic == NULL) || (expected == NULL))
  {
    return 0U;
  }

  expected_len = strlen(expected);
  if (topic_len != expected_len)
  {
    return 0U;
  }

  return (memcmp(topic, expected, expected_len) == 0) ? 1U : 0U;
}

static void OneNet_CopyTextForLog(const char *src,
                                  uint16_t src_len,
                                  char *dst,
                                  size_t dst_size)
{
  size_t copy_len = 0U;

  if ((dst == NULL) || (dst_size == 0U))
  {
    return;
  }

  memset(dst, 0, dst_size);
  if ((src == NULL) || (src_len == 0U))
  {
    return;
  }

  copy_len = src_len;
  if (copy_len >= dst_size)
  {
    copy_len = dst_size - 1U;
  }

  memcpy(dst, src, copy_len);
  dst[copy_len] = '\0';
}

static void OneNet_CopyDownlink(const char *topic,
                                uint16_t topic_len,
                                const char *payload,
                                uint16_t payload_len)
{
  uint16_t copy_topic_len = topic_len;
  uint16_t copy_payload_len = payload_len;

  if (copy_topic_len >= ONENET_DOWNLINK_TOPIC_SIZE)
  {
    copy_topic_len = (uint16_t)(ONENET_DOWNLINK_TOPIC_SIZE - 1U);
  }
  if (copy_payload_len >= ONENET_DOWNLINK_PAYLOAD_SIZE)
  {
    copy_payload_len = (uint16_t)(ONENET_DOWNLINK_PAYLOAD_SIZE - 1U);
  }

  memset(g_onenet_last_topic, 0, sizeof(g_onenet_last_topic));
  memset(g_onenet_last_payload, 0, sizeof(g_onenet_last_payload));

  if ((topic != NULL) && (copy_topic_len > 0U))
  {
    memcpy(g_onenet_last_topic, topic, copy_topic_len);
  }
  if ((payload != NULL) && (copy_payload_len > 0U))
  {
    memcpy(g_onenet_last_payload, payload, copy_payload_len);
  }

  g_onenet_last_topic[copy_topic_len] = '\0';
  g_onenet_last_payload[copy_payload_len] = '\0';
  g_onenet_downlink_count++;
}

static const char *OneNet_SkipJsonSpaces(const char *text)
{
  const char *cursor = text;

  while ((cursor != NULL) &&
         ((*cursor == ' ') || (*cursor == '\t') ||
          (*cursor == '\r') || (*cursor == '\n')))
  {
    cursor++;
  }

  return cursor;
}

static const char *OneNet_FindJsonValueStart(const char *json, const char *key)
{
  char pattern[24];
  const char *match = NULL;
  const char *colon = NULL;
  int pattern_len = 0;

  if ((json == NULL) || (key == NULL))
  {
    return NULL;
  }

  pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if ((pattern_len <= 0) || ((size_t)pattern_len >= sizeof(pattern)))
  {
    return NULL;
  }

  match = strstr(json, pattern);
  if (match == NULL)
  {
    return NULL;
  }

  colon = strchr(match + pattern_len, ':');
  if (colon == NULL)
  {
    return NULL;
  }

  return OneNet_SkipJsonSpaces(colon + 1);
}

static uint8_t OneNet_ParseJsonTextValue(const char *value_start,
                                         char *output,
                                         size_t output_size)
{
  const char *begin = NULL;
  const char *end = NULL;
  size_t copy_len = 0U;

  if ((value_start == NULL) || (output == NULL) || (output_size == 0U))
  {
    return 0U;
  }

  if (*value_start == '\"')
  {
    begin = value_start + 1;
    end = begin;
    while ((*end != '\0') && (*end != '\"'))
    {
      end++;
    }
    if (*end != '\"')
    {
      return 0U;
    }
  }
  else
  {
    begin = value_start;
    end = begin;
    while ((*end != '\0') &&
           (*end != ',') &&
           (*end != '}') &&
           (*end != ' ') &&
           (*end != '\t') &&
           (*end != '\r') &&
           (*end != '\n'))
    {
      end++;
    }
  }

  copy_len = (size_t)(end - begin);
  if ((copy_len == 0U) || (copy_len >= output_size))
  {
    return 0U;
  }

  memcpy(output, begin, copy_len);
  output[copy_len] = '\0';
  return 1U;
}

static uint8_t OneNet_ParseJsonIntValue(const char *value_start, int32_t *value)
{
  char number_text[24];
  char *end_ptr = NULL;
  long parsed = 0L;

  if ((value_start == NULL) || (value == NULL))
  {
    return 0U;
  }

  if (OneNet_ParseJsonTextValue(value_start, number_text, sizeof(number_text)) == 0U)
  {
    return 0U;
  }

  parsed = strtol(number_text, &end_ptr, 10);
  if ((end_ptr == NULL) || (*end_ptr != '\0'))
  {
    return 0U;
  }

  *value = (int32_t)parsed;
  return 1U;
}

static uint8_t OneNet_ParseRequestId(const char *json,
                                     char *request_id,
                                     size_t request_id_size)
{
  const char *value_start = OneNet_FindJsonValueStart(json, "id");
  return OneNet_ParseJsonTextValue(value_start, request_id, request_id_size);
}

static uint8_t OneNet_ParsePropertyIntValue(const char *json,
                                            const char *property_key,
                                            int32_t *value)
{
  const char *params_start = NULL;
  const char *value_start = NULL;
  const char *nested_value_start = NULL;

  if ((json == NULL) || (property_key == NULL) || (value == NULL))
  {
    return 0U;
  }

  params_start = OneNet_FindJsonValueStart(json, "params");
  if (params_start != NULL)
  {
    value_start = OneNet_FindJsonValueStart(params_start, property_key);
  }

  if (value_start == NULL)
  {
    value_start = OneNet_FindJsonValueStart(json, property_key);
  }

  if (value_start == NULL)
  {
    return 0U;
  }

  if (*value_start == '{')
  {
    nested_value_start = OneNet_FindJsonValueStart(value_start, "value");
    return OneNet_ParseJsonIntValue(nested_value_start, value);
  }

  return OneNet_ParseJsonIntValue(value_start, value);
}

static uint8_t OneNet_ParseTestValue(const char *json, int32_t *test_value)
{
  return OneNet_ParsePropertyIntValue(json, ONENET_DP_TEST_KEY, test_value);
}

static uint8_t OneNet_IsDecodedTestValueValid(int32_t action_id, int32_t score)
{
  if ((action_id < ONENET_TEST_ACTION_ID_MIN) || (action_id > ONENET_TEST_ACTION_ID_MAX))
  {
    return 0U;
  }

  if ((score < ONENET_TEST_SCORE_MIN) || (score > ONENET_TEST_SCORE_MAX))
  {
    return 0U;
  }

  return 1U;
}

static uint8_t OneNet_PublishRaw(const char *topic,
                                 const char *payload,
                                 uint32_t payload_len,
                                 uint8_t packet_fail_status,
                                 uint8_t send_fail_status)
{
  MQTT_PACKET_STRUCTURE packet = {0};

  if (MQTT_PacketPublish(MQTT_PUBLISH_ID,
                         topic,
                         payload,
                         payload_len,
                         MQTT_QOS_LEVEL0,
                         0,
                         1,
                         &packet) != 0U)
  {
    g_onenet_last_status = packet_fail_status;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = send_fail_status;
    return 0U;
  }

  MQTT_DeleteBuffer(&packet);
  g_onenet_last_status = ONENET_STATUS_OK;
  return 1U;
}

static uint8_t OneNet_SendPropertySetReply(const char *request_id,
                                           uint16_t code,
                                           const char *message)
{
  char payload[ONENET_REPLY_PAYLOAD_SIZE];
  int payload_len = 0;

  if ((request_id == NULL) || (message == NULL))
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY invalid args\r\n");
    g_onenet_last_status = ONENET_STATUS_FAIL_REPLY_PACKET;
    return 0U;
  }

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%s\",\"code\":%u,\"msg\":\"%s\"}",
                         request_id,
                         (unsigned int)code,
                         message);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY payload build fail id=%s\r\n", request_id);
    g_onenet_last_status = ONENET_STATUS_FAIL_REPLY_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] SET_REPLY TX topic=%s payload=%s\r\n",
               ONENET_TOPIC_PROP_SET_REPLY,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_SET_REPLY,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_REPLY_PACKET,
                        ONENET_STATUS_FAIL_REPLY_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY send fail id=%s code=%u\r\n",
                 request_id,
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  Debug_Printf("[MQTT] SET_REPLY OK id=%s\r\n", request_id);
  return 1U;
}

uint8_t OneNet_PostTestConfidenceValue(int32_t test_value, float confidence_value)
{
  char payload[ONENET_REPLY_PAYLOAD_SIZE + 96U];
  int payload_len = 0;

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%ld},\"%s\":{\"value\":%.3f}}}",
                         (unsigned long)g_onenet_publish_seq,
                         ONENET_DP_TEST_KEY,
                         (long)test_value,
                         ONENET_DP_CONFIDENCE_KEY,
                         (double)confidence_value);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] PROP POST test+confidence payload build fail test=%ld conf=%.3f\r\n",
                 (long)test_value,
                 (double)confidence_value);
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] PROP POST test+confidence TX topic=%s payload=%s\r\n",
               ONENET_TOPIC_PROP_POST,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] PROP POST test+confidence send fail test=%ld conf=%.3f code=%u\r\n",
                 (long)test_value,
                 (double)confidence_value,
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  g_onenet_publish_seq++;
  Debug_Printf("[MQTT] PROP POST test+confidence OK test=%ld conf=%.3f\r\n",
               (long)test_value,
               (double)confidence_value);
  return 1U;
}

uint8_t OneNet_PostActionKindValue(int32_t action_kind_value)
{
  return OneNet_PostIntPropertyValue(ONENET_DP_ACTION_KIND_KEY,
                                     action_kind_value,
                                     "action_kind");
}

uint8_t OneNet_PostFallAlarmValue(int32_t fall_alarm_value)
{
  return OneNet_PostIntPropertyValue(ONENET_DP_FALL_ALARM_KEY,
                                     fall_alarm_value,
                                     "fall_alarm");
}

uint8_t OneNet_TakeLastValidTestValue(int32_t *test_value)
{
  if ((test_value == NULL) || (g_onenet_has_valid_test_value == 0U))
  {
    return 0U;
  }

  *test_value = g_onenet_last_valid_test_value;
  g_onenet_has_valid_test_value = 0U;
  return 1U;
}

void OneNet_ActivateFallAlarm(void)
{
  if (g_onenet_fall_alarm_active != 0U)
  {
    return;
  }

  g_onenet_fall_alarm_active = 1U;
  g_onenet_pending_fall_alarm_value = 1;
  g_onenet_fall_alarm_post_pending = 1U;
}

void OneNet_ClearFallAlarm(void)
{
  if ((g_onenet_fall_alarm_active == 0U) &&
      (g_onenet_pending_fall_alarm_value == 0) &&
      (g_onenet_fall_alarm_post_pending == 0U))
  {
    return;
  }

  g_onenet_fall_alarm_active = 0U;
  g_onenet_pending_fall_alarm_value = 0;
  g_onenet_fall_alarm_post_pending = 1U;
}

uint8_t OneNet_IsFallAlarmActive(void)
{
  return g_onenet_fall_alarm_active;
}

void OneNet_GetTrainPlanSnapshot(OneNetTrainPlanSnapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  snapshot->elbow_flex_count = g_onenet_train_plan.elbow_flex_count;
  snapshot->front_raise_count = g_onenet_train_plan.front_raise_count;
  snapshot->shoulder_raise_count = g_onenet_train_plan.shoulder_raise_count;
  snapshot->side_raise_count = g_onenet_train_plan.side_raise_count;
}

void OneNet_GetTrainDisplaySnapshot(OneNetTrainPlanSnapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  snapshot->elbow_flex_count = g_onenet_train_display.elbow_flex_count;
  snapshot->front_raise_count = g_onenet_train_display.front_raise_count;
  snapshot->shoulder_raise_count = g_onenet_train_display.shoulder_raise_count;
  snapshot->side_raise_count = g_onenet_train_display.side_raise_count;
}

void OneNet_UpdateTrainDisplayByAction(int32_t action_label)
{
  int32_t *display_slot = OneNet_GetTrainDisplaySlotByAction(action_label);

  if (display_slot == NULL)
  {
    Debug_Printf("[MQTT] TRAIN DISPLAY ignore action=%s\r\n",
                 MotionAi_LabelName((motion_label_t)action_label));
    return;
  }

  if (*display_slot < 0)
  {
    (*display_slot)++;
  }

  Debug_Printf("[MQTT] TRAIN DISPLAY action=%s remain elbow=%ld front=%ld shoulder=%ld side=%ld\r\n",
               MotionAi_LabelName((motion_label_t)action_label),
               (long)g_onenet_train_display.elbow_flex_count,
               (long)g_onenet_train_display.front_raise_count,
               (long)g_onenet_train_display.shoulder_raise_count,
               (long)g_onenet_train_display.side_raise_count);
}

uint8_t OneNet_PostPendingTrainPlan(void)
{
  if (g_onenet_train_plan_post_pending == 0U)
  {
    return 1U;
  }

  if (OneNet_PostTrainPlan() == 0U)
  {
    return 0U;
  }

  g_onenet_train_plan_post_pending = 0U;
  return 1U;
}

uint8_t OneNet_PostPendingDoorState(void)
{
  if (g_onenet_door_state_post_pending == 0U)
  {
    return 1U;
  }

  if (OneNet_PostIntPropertyValue(ONENET_DP_OPEN_KEY,
                                  g_onenet_pending_door_state_value,
                                  "open") == 0U)
  {
    return 0U;
  }

  g_onenet_door_state_post_pending = 0U;
  return 1U;
}

uint8_t OneNet_PostPendingFallAlarm(void)
{
  int32_t pending_value = g_onenet_pending_fall_alarm_value;

  if (g_onenet_fall_alarm_post_pending == 0U)
  {
    return 1U;
  }

  if (OneNet_PostFallAlarmValue(pending_value) == 0U)
  {
    return 0U;
  }

  if (g_onenet_pending_fall_alarm_value == pending_value)
  {
    g_onenet_fall_alarm_post_pending = 0U;
  }

  return 1U;
}

static void OneNet_InitPropSetContext(onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->reply_code = ONENET_REPLY_CODE_OK;
  ctx->reply_message = "success";
}

static void OneNet_HandleTrainPlanProperty(onenet_prop_set_context_t *ctx)
{
  int32_t plan_value = 0;

  if (ctx == NULL)
  {
    return;
  }

  if (OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                   ONENET_DP_ELBOW_FLEX_COUNT_KEY,
                                   &plan_value) != 0U)
  {
    g_onenet_train_plan.elbow_flex_count = plan_value;
    ctx->has_plan_update = 1U;
    Debug_Printf("[MQTT] PLAN SET elbow_flex_count=%ld\r\n", (long)plan_value);
  }
  if (OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                   ONENET_DP_FRONT_RAISE_COUNT_KEY,
                                   &plan_value) != 0U)
  {
    g_onenet_train_plan.front_raise_count = plan_value;
    ctx->has_plan_update = 1U;
    Debug_Printf("[MQTT] PLAN SET front_raise_count=%ld\r\n", (long)plan_value);
  }
  if (OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                   ONENET_DP_SHOULDER_RAISE_COUNT_KEY,
                                   &plan_value) != 0U)
  {
    g_onenet_train_plan.shoulder_raise_count = plan_value;
    ctx->has_plan_update = 1U;
    Debug_Printf("[MQTT] PLAN SET shoulder_raise_count=%ld\r\n", (long)plan_value);
  }
  if (OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                   ONENET_DP_SIDE_RAISE_COUNT_KEY,
                                   &plan_value) != 0U)
  {
    g_onenet_train_plan.side_raise_count = plan_value;
    ctx->has_plan_update = 1U;
    Debug_Printf("[MQTT] PLAN SET side_raise_count=%ld\r\n", (long)plan_value);
  }

  if (ctx->has_plan_update != 0U)
  {
    OneNet_ResetTrainDisplayFromPlan();
    Debug_Printf("[MQTT] PLAN DISPLAY RESET elbow=%ld front=%ld shoulder=%ld side=%ld\r\n",
                 (long)g_onenet_train_display.elbow_flex_count,
                 (long)g_onenet_train_display.front_raise_count,
                 (long)g_onenet_train_display.shoulder_raise_count,
                 (long)g_onenet_train_display.side_raise_count);
  }
}

static void OneNet_HandleDoorProperty(onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->has_open_value = OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                                     ONENET_DP_OPEN_KEY,
                                                     &ctx->open_value);
  if (ctx->has_open_value == 0U)
  {
    return;
  }

  ctx->open_value_valid = Servo_SetDoorByCloudValue(ctx->open_value);
  if (ctx->open_value_valid != 0U)
  {
    Debug_Printf("[MQTT] DOOR SET open=%ld applied_state=%u\r\n",
                 (long)ctx->open_value,
                 (unsigned int)Servo_GetDoorState());
  }
  else
  {
    ctx->reply_code = ONENET_REPLY_CODE_BAD_REQUEST;
    ctx->reply_message = "open must be 0 or 1";
    Debug_Printf("[MQTT][WARN] DOOR SET invalid open=%ld\r\n",
                 (long)ctx->open_value);
  }
}

static void OneNet_HandleFallAlarmProperty(onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->has_fall_alarm_value = OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                                           ONENET_DP_FALL_ALARM_KEY,
                                                           &ctx->fall_alarm_value);
  if (ctx->has_fall_alarm_value == 0U)
  {
    return;
  }

  if ((ctx->fall_alarm_value == 0) || (ctx->fall_alarm_value == 1))
  {
    ctx->fall_alarm_value_valid = 1U;
    Debug_Printf("[MQTT] FALL SET fall_alarm=%ld\r\n",
                 (long)ctx->fall_alarm_value);
  }
  else
  {
    ctx->reply_code = ONENET_REPLY_CODE_BAD_REQUEST;
    ctx->reply_message = "fall_alarm must be 0 or 1";
    Debug_Printf("[MQTT][WARN] FALL SET invalid fall_alarm=%ld\r\n",
                 (long)ctx->fall_alarm_value);
  }
}

static void OneNet_HandleStartProperty(onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->has_start_value = OneNet_ParsePropertyIntValue(g_onenet_last_payload,
                                                      ONENET_DP_START_KEY,
                                                      &ctx->start_value);
  if (ctx->has_start_value == 0U)
  {
    return;
  }

  if (ctx->start_value == 1)
  {
    ctx->start_value_valid = 1U;
    Debug_Printf("[MQTT] START SET start=%ld\r\n",
                 (long)ctx->start_value);
  }
  else
  {
    ctx->reply_code = ONENET_REPLY_CODE_BAD_REQUEST;
    ctx->reply_message = "start must be 1";
    Debug_Printf("[MQTT][WARN] START SET invalid start=%ld\r\n",
                 (long)ctx->start_value);
  }
}

static void OneNet_HandleTestProperty(onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->has_test_value = OneNet_ParseTestValue(g_onenet_last_payload, &ctx->test_value);
}

static void OneNet_LogPropertySetSummary(const onenet_prop_set_context_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  Debug_Printf("[MQTT] PROP SET payload=%s\r\n", g_onenet_last_payload);

  if (ctx->has_test_value != 0U)
  {
    Debug_Printf("[MQTT] PROP SET test=%ld\r\n", (long)ctx->test_value);
  }
  if (ctx->has_plan_update != 0U)
  {
    Debug_Printf("[MQTT] PROP SET plan update parsed\r\n");
  }
  if (ctx->has_open_value != 0U)
  {
    Debug_Printf("[MQTT] PROP SET open=%ld\r\n", (long)ctx->open_value);
  }
  if (ctx->has_fall_alarm_value != 0U)
  {
    Debug_Printf("[MQTT] PROP SET fall_alarm=%ld\r\n", (long)ctx->fall_alarm_value);
  }
  if (ctx->has_start_value != 0U)
  {
    Debug_Printf("[MQTT] PROP SET start=%ld\r\n", (long)ctx->start_value);
  }
  if ((ctx->has_test_value == 0U) &&
      (ctx->has_plan_update == 0U) &&
      (ctx->has_open_value == 0U) &&
      (ctx->has_fall_alarm_value == 0U) &&
      (ctx->has_start_value == 0U))
  {
    Debug_Printf("[MQTT][WARN] PROP SET no known property parsed\r\n");
  }
}

static void OneNet_ApplyDoorProperty(const onenet_prop_set_context_t *ctx)
{
  if ((ctx == NULL) ||
      (ctx->has_open_value == 0U) ||
      (ctx->open_value_valid == 0U))
  {
    return;
  }

  g_onenet_pending_door_state_value = (int32_t)Servo_GetDoorState();
  g_onenet_door_state_post_pending = 1U;
  Debug_Printf("[MQTT] open cached, defer post to net task value=%ld\r\n",
               (long)g_onenet_pending_door_state_value);
}

static void OneNet_ApplyFallAlarmProperty(const onenet_prop_set_context_t *ctx)
{
  if ((ctx == NULL) ||
      (ctx->has_fall_alarm_value == 0U) ||
      (ctx->fall_alarm_value_valid == 0U))
  {
    return;
  }

  if (ctx->fall_alarm_value != 0)
  {
    OneNet_ActivateFallAlarm();
  }
  else
  {
    Motion_RequestClear();
  }
}

static void OneNet_ApplyStartProperty(const onenet_prop_set_context_t *ctx)
{
  if ((ctx == NULL) ||
      (ctx->has_start_value == 0U) ||
      (ctx->start_value_valid == 0U))
  {
    return;
  }

  Motion_RequestStart();
}

static void OneNet_ApplyTestProperty(onenet_prop_set_context_t *ctx)
{
  if ((ctx == NULL) || (ctx->has_test_value == 0U))
  {
    return;
  }

  ctx->decoded_action_id = ctx->test_value / 10;
  ctx->decoded_score = ctx->test_value % 10;
  ctx->test_value_valid = OneNet_IsDecodedTestValueValid(ctx->decoded_action_id,
                                                         ctx->decoded_score);
  Debug_Printf("[MQTT] PROP SET decoded action_id=%ld score=%ld valid=%u\r\n",
               (long)ctx->decoded_action_id,
               (long)ctx->decoded_score,
               (unsigned int)ctx->test_value_valid);

  if (ctx->test_value_valid != 0U)
  {
    g_onenet_last_valid_test_value = ctx->test_value;
    g_onenet_has_valid_test_value = 1U;
    MotionAi_SetDemoOverride(ctx->test_value,
                             ctx->decoded_action_id,
                             ctx->decoded_score);
  }
  else
  {
    MotionAi_ClearDemoOverride();
    Debug_Printf("[MQTT][WARN] PROP SET invalid test=%ld, demo override cleared\r\n",
                 (long)ctx->test_value);
  }

  Debug_Printf("[MQTT] PROP SET test cached, defer post until TEST_DONE\r\n");
}

static void OneNet_ApplyTrainPlanProperty(const onenet_prop_set_context_t *ctx)
{
  if ((ctx == NULL) || (ctx->has_plan_update == 0U))
  {
    return;
  }

  g_onenet_train_plan_post_pending = 1U;
  Debug_Printf("[MQTT] PLAN SET cached, defer post to net task\r\n");
}

static void OneNet_ApplyPropertySetActions(onenet_prop_set_context_t *ctx)
{
  OneNet_ApplyDoorProperty(ctx);
  OneNet_ApplyFallAlarmProperty(ctx);
  OneNet_ApplyStartProperty(ctx);
  OneNet_ApplyTestProperty(ctx);
  OneNet_ApplyTrainPlanProperty(ctx);
}

static void OneNet_HandlePropertySet(const char *topic,
                                     uint16_t topic_len,
                                     const char *payload,
                                     uint16_t payload_len)
{
  onenet_prop_set_context_t ctx;

  /* 属性下发先解析成上下文，再统一应用动作，避免协议解析和业务控制交叉。 */
  OneNet_InitPropSetContext(&ctx);
  Debug_Printf("[MQTT] PROP SET matched\r\n");
  OneNet_CopyDownlink(topic, topic_len, payload, payload_len);

  ctx.has_request_id = OneNet_ParseRequestId(g_onenet_last_payload,
                                             ctx.request_id,
                                             sizeof(ctx.request_id));
  OneNet_HandleTestProperty(&ctx);
  OneNet_HandleTrainPlanProperty(&ctx);
  OneNet_HandleDoorProperty(&ctx);
  OneNet_HandleFallAlarmProperty(&ctx);
  OneNet_HandleStartProperty(&ctx);
  OneNet_LogPropertySetSummary(&ctx);

  if (ctx.has_request_id != 0U)
  {
    Debug_Printf("[MQTT] PROP SET reply id=%s\r\n", ctx.request_id);
    if (OneNet_SendPropertySetReply(ctx.request_id,
                                    ctx.reply_code,
                                    ctx.reply_message) == 0U)
    {
      g_onenet_session_error = 1U;
      return;
    }

    OneNet_ApplyPropertySetActions(&ctx);
  }
  else
  {
    Debug_Printf("[MQTT][ERR] PROP SET missing id\r\n");
    g_onenet_last_status = ONENET_STATUS_FAIL_RX_PARSE;
  }
}

static void OneNet_HandlePublishPacket(const uint8_t *packet)
{
  char *topic = NULL;
  char *payload = NULL;
  char topic_text[ONENET_DOWNLINK_TOPIC_SIZE];
  char payload_text[ONENET_DOWNLINK_PAYLOAD_SIZE];
  uint16_t topic_len = 0U;
  uint16_t payload_len = 0U;
  uint8_t qos = 0U;
  uint16_t pkt_id = 0U;
  uint8_t parse_status = 0U;

  parse_status = MQTT_UnPacketPublish((uint8_t *)packet,
                                      &topic,
                                      &topic_len,
                                      &payload,
                                      &payload_len,
                                      &qos,
                                      &pkt_id);
  if (parse_status != 0U)
  {
    Debug_Printf("[MQTT] PUBLISH parse fail code=%u\r\n",
                 (unsigned int)parse_status);
    g_onenet_session_error = 1U;
    g_onenet_last_status = ONENET_STATUS_FAIL_RX_PARSE;
    return;
  }

  OneNet_CopyTextForLog(topic, topic_len, topic_text, sizeof(topic_text));
  OneNet_CopyTextForLog(payload, payload_len, payload_text, sizeof(payload_text));

  Debug_Printf("[MQTT] RX PUBLISH topic=%s qos=%u pkt_id=%u payload_len=%u\r\n",
               topic_text,
               (unsigned int)qos,
               (unsigned int)pkt_id,
               (unsigned int)payload_len);
  Debug_Printf("[MQTT] RX PUBLISH payload=%s\r\n", payload_text);

  if (OneNet_TopicMatches(topic, topic_len, ONENET_TOPIC_PROP_SET) != 0U)
  {
    OneNet_HandlePropertySet(topic, topic_len, payload, payload_len);
  }
  else
  {
    Debug_Printf("[MQTT] PUBLISH topic mismatch expect=%s got=%s\r\n",
                 ONENET_TOPIC_PROP_SET,
                 topic_text);
  }
}

uint8_t OneNet_DevLink(void)
{
  MQTT_PACKET_STRUCTURE packet = {0};
  uint8_t *data_ptr = NULL;
  uint8_t connack = 0U;

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_last_connack_code = 0U;
  if (MQTT_PacketConnect(ONENET_USERNAME,
                         ONENET_PASSWORD,
                         ONENET_CLIENT_ID,
                         ONENET_KEEP_ALIVE_SECONDS,
                         1U,
                         MQTT_QOS_LEVEL0,
                         NULL,
                         NULL,
                         0,
                         &packet) != 0U)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_PACKET;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_SEND;
    return 0U;
  }
  MQTT_DeleteBuffer(&packet);

  data_ptr = ESP8266_GetIPD(ONENET_PACKET_TIMEOUT_MS);
  if (data_ptr == NULL)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_WAIT;
    return 0U;
  }

  if (MQTT_UnPacketRecv(data_ptr) != MQTT_PKT_CONNACK)
  {
    ESP8266_Clear();
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_TYPE;
    return 0U;
  }

  connack = MQTT_UnPacketConnectAck(data_ptr);
  ESP8266_Clear();
  if (connack != 0U)
  {
    g_onenet_last_connack_code = connack;
    g_onenet_session_error = 1U;
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_ACK;
    return 0U;
  }

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_last_connack_code = 0U;
  return 1U;
}

uint8_t OneNet_Subscribe(void)
{
  MQTT_PACKET_STRUCTURE packet = {0};
  static const char *topics[] =
  {
    ONENET_TOPIC_PROP_SET
  };

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_subscribe_ready = 0U;
  g_onenet_last_subscribe_topic = ONENET_SUBSCRIBE_GROUP_NAME;

  if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID,
                           MQTT_QOS_LEVEL0,
                           topics,
                           (uint8_t)(sizeof(topics) / sizeof(topics[0])),
                           &packet) != 0U)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_SUB_PACKET;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = ONENET_STATUS_FAIL_SUB_SEND;
    return 0U;
  }

  MQTT_DeleteBuffer(&packet);
  return 1U;
}

uint8_t OneNet_Publish(void)
{
  char payload[160];
  int payload_len = 0;

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"params\":{\"%s\":{\"value\":%d},\"%s\":{\"value\":%d}}}",
                         (unsigned long)g_onenet_publish_seq,
                         ONENET_DP_HEART_RATE_KEY,
                         ONENET_FIXED_HEART_RATE,
                         ONENET_DP_BLOOD_OXYGEN_KEY,
                         ONENET_FIXED_SPO2);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    return 0U;
  }

  g_onenet_publish_seq++;
  return 1U;
}

void OneNet_RevPro(const uint8_t *packet)
{
  uint8_t type = 0U;

  /* 网络任务收到 MQTT 包后只进入这里分发，外层不理解具体 topic/payload。 */
  if (packet == NULL)
  {
    return;
  }

  type = MQTT_UnPacketRecv((uint8_t *)packet);
  if ((type < MQTT_PKT_CONNECT) || (type > MQTT_PKT_DISCONNECT))
  {
    Debug_Printf("[MQTT] RX INVALID first=0x%02X\r\n",
                 (unsigned int)packet[0]);
    ESP8266_DebugDumpCurrentRx("MQTT_INVALID", 48U);
    return;
  }

  Debug_Printf("[MQTT] RX TYPE=%u\r\n", (unsigned int)type);
  switch (type)
  {
    case MQTT_PKT_CONNACK:
      Debug_Printf("[MQTT] RX CONNACK\r\n");
      if (MQTT_UnPacketConnectAck((uint8_t *)packet) != 0U)
      {
        g_onenet_session_error = 1U;
      }
      break;

    case MQTT_PKT_SUBACK:
      Debug_Printf("[MQTT] RX SUBACK\r\n");
      if (MQTT_UnPacketSubscribe((uint8_t *)packet) != 0U)
      {
        g_onenet_session_error = 1U;
        g_onenet_last_status = ONENET_STATUS_FAIL_SUB_ACK;
        g_onenet_subscribe_ready = 0U;
      }
      else
      {
        g_onenet_last_status = ONENET_STATUS_OK;
        g_onenet_subscribe_ready = 1U;
      }
      break;

    case MQTT_PKT_PUBACK:
      Debug_Printf("[MQTT] RX PUBACK\r\n");
      (void)MQTT_UnPacketPublishAck((uint8_t *)packet);
      break;

    case MQTT_PKT_PINGRESP:
      Debug_Printf("[MQTT] RX PINGRESP\r\n");
      break;

    case MQTT_PKT_PUBLISH:
      OneNet_HandlePublishPacket(packet);
      break;

    default:
      Debug_Printf("[MQTT] RX UNHANDLED TYPE=%u\r\n", (unsigned int)type);
      break;
  }
}

uint8_t OneNet_HasSessionError(void)
{
  return g_onenet_session_error;
}

void OneNet_ClearSessionError(void)
{
  g_onenet_session_error = 0U;
}

void OneNet_ResetSubscribeState(void)
{
  g_onenet_subscribe_ready = 0U;
}

uint8_t OneNet_IsSubscribeReady(void)
{
  return g_onenet_subscribe_ready;
}

uint8_t OneNet_GetLastStatus(void)
{
  return g_onenet_last_status;
}

uint8_t OneNet_GetLastConnAckCode(void)
{
  return g_onenet_last_connack_code;
}

const char *OneNet_GetLastSubscribeTopic(void)
{
  return g_onenet_last_subscribe_topic;
}
