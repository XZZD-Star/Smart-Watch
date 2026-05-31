#ifndef __ONENET_H__
#define __ONENET_H__

#include <stdint.h>

#define ONENET_PRODUCT_ID               "S9U9FY9ZdS"
#define ONENET_DEVICE_NAME              "Bracelet"
#define ONENET_CLIENT_ID                ONENET_DEVICE_NAME
#define ONENET_USERNAME                 ONENET_PRODUCT_ID
#define ONENET_PASSWORD                 "version=2018-10-31&res=products%2FS9U9FY9ZdS%2Fdevices%2FBracelet&et=1805863774&method=md5&sign=h%2F8qRCOICNVzmUC0cTq5Bg%3D%3D"

#define ONENET_KEEP_ALIVE_SECONDS       60U
#define ONENET_PUBLISH_INTERVAL_MS    5000U

#define ONENET_DP_HEART_RATE_KEY        "HeartRate"
#define ONENET_DP_BLOOD_OXYGEN_KEY      "BloodOxygen"
#define ONENET_DP_TEST_KEY              "test"
#define ONENET_DP_OPEN_KEY              "open"
#define ONENET_DP_ACTION_KIND_KEY       "action_kind"
#define ONENET_DP_CONFIDENCE_KEY        "confidence"
#define ONENET_DP_START_KEY             "start"
#define ONENET_DP_FALL_ALARM_KEY        "fall_alarm"
#define ONENET_DP_ELBOW_FLEX_COUNT_KEY  "elbow_flex_count"
#define ONENET_DP_FRONT_RAISE_COUNT_KEY "front_raise_count"
#define ONENET_DP_SHOULDER_RAISE_COUNT_KEY "shoulder_raise_count"
#define ONENET_DP_SIDE_RAISE_COUNT_KEY  "side_raise_count"

#define ONENET_FIXED_HEART_RATE          78U
#define ONENET_FIXED_SPO2                98U

#define ONENET_TOPIC_PROP_POST          "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/post"
#define ONENET_TOPIC_PROP_SET           "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set"
#define ONENET_TOPIC_PROP_SET_REPLY     "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set_reply"

#define ONENET_STATUS_OK                    0U
#define ONENET_STATUS_FAIL_CONNECT_PACKET   1U
#define ONENET_STATUS_FAIL_CONNECT_SEND     2U
#define ONENET_STATUS_FAIL_CONNECT_WAIT     3U
#define ONENET_STATUS_FAIL_CONNECT_TYPE     4U
#define ONENET_STATUS_FAIL_CONNECT_ACK      5U
#define ONENET_STATUS_FAIL_SUB_PACKET       6U
#define ONENET_STATUS_FAIL_SUB_SEND         7U
#define ONENET_STATUS_FAIL_SUB_WAIT         8U
#define ONENET_STATUS_FAIL_SUB_ACK          9U
#define ONENET_STATUS_FAIL_PUBLISH_PACKET  10U
#define ONENET_STATUS_FAIL_PUBLISH_SEND    11U
#define ONENET_STATUS_FAIL_RX_PARSE        12U
#define ONENET_STATUS_FAIL_REPLY_PACKET    13U
#define ONENET_STATUS_FAIL_REPLY_SEND      14U

typedef struct
{
  int32_t elbow_flex_count;
  int32_t front_raise_count;
  int32_t shoulder_raise_count;
  int32_t side_raise_count;
} OneNetTrainPlanSnapshot_t;

uint8_t OneNet_DevLink(void);
uint8_t OneNet_Subscribe(void);
uint8_t OneNet_Publish(void);
uint8_t OneNet_PostTestConfidenceValue(int32_t test_value, float confidence_value);
uint8_t OneNet_PostActionKindValue(int32_t action_kind_value);
uint8_t OneNet_PostFallAlarmValue(int32_t fall_alarm_value);
uint8_t OneNet_PostPendingFallAlarm(void);
uint8_t OneNet_PostPendingTrainPlan(void);
uint8_t OneNet_PostPendingDoorState(void);
uint8_t OneNet_TakeLastValidTestValue(int32_t *test_value);
void OneNet_ActivateFallAlarm(void);
void OneNet_ClearFallAlarm(void);
uint8_t OneNet_IsFallAlarmActive(void);
void OneNet_GetTrainPlanSnapshot(OneNetTrainPlanSnapshot_t *snapshot);
void OneNet_GetTrainDisplaySnapshot(OneNetTrainPlanSnapshot_t *snapshot);
void OneNet_UpdateTrainDisplayByAction(int32_t action_label);
void OneNet_RevPro(const uint8_t *packet);
void OneNet_ResetSubscribeState(void);
uint8_t OneNet_IsSubscribeReady(void);

uint8_t OneNet_HasSessionError(void);
void OneNet_ClearSessionError(void);
uint8_t OneNet_GetLastStatus(void);
uint8_t OneNet_GetLastConnAckCode(void);
const char *OneNet_GetLastSubscribeTopic(void);

#endif /* __ONENET_H__ */
