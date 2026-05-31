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

/* 网络任务调用：发送 MQTT CONNECT 并等待云端确认。 */
uint8_t OneNet_DevLink(void);
/* 网络任务调用：订阅 OneNet 属性下发 topic。 */
uint8_t OneNet_Subscribe(void);
/* 网络任务周期调用：上报固定健康数据并保持云端在线。 */
uint8_t OneNet_Publish(void);
/* 网络任务调用：上报一次测试编号和识别置信度。 */
uint8_t OneNet_PostTestConfidenceValue(int32_t test_value, float confidence_value);
/* 网络任务调用：上报动作类别编码。 */
uint8_t OneNet_PostActionKindValue(int32_t action_kind_value);
/* 网络任务调用：上报跌倒告警值。 */
uint8_t OneNet_PostFallAlarmValue(int32_t fall_alarm_value);
/* 网络任务调用：若有待上报跌倒告警则发送。 */
uint8_t OneNet_PostPendingFallAlarm(void);
/* 网络任务调用：若有待上报训练计划则发送。 */
uint8_t OneNet_PostPendingTrainPlan(void);
/* 网络任务调用：若有待上报门状态则发送。 */
uint8_t OneNet_PostPendingDoorState(void);
/* 运动任务调用：取走最近一次云端下发且合法的 test 值。 */
uint8_t OneNet_TakeLastValidTestValue(int32_t *test_value);
/* 运动/云控调用：置位本地跌倒告警并排队上报。 */
void OneNet_ActivateFallAlarm(void);
/* 运动/云控调用：清除本地跌倒告警并排队上报。 */
void OneNet_ClearFallAlarm(void);
/* 运动任务调用：查询当前跌倒告警是否处于激活状态。 */
uint8_t OneNet_IsFallAlarmActive(void);
/* 屏幕任务调用：读取云端训练计划目标快照。 */
void OneNet_GetTrainPlanSnapshot(OneNetTrainPlanSnapshot_t *snapshot);
/* 屏幕任务调用：读取用于训练页显示的剩余次数快照。 */
void OneNet_GetTrainDisplaySnapshot(OneNetTrainPlanSnapshot_t *snapshot);
/* 运动任务调用：动作完成后扣减训练页显示次数。 */
void OneNet_UpdateTrainDisplayByAction(int32_t action_label);
/* 网络任务调用：解析一包 MQTT 下行数据。 */
void OneNet_RevPro(const uint8_t *packet);
/* 网络任务重连前调用：清除订阅确认状态。 */
void OneNet_ResetSubscribeState(void);
/* 网络任务调用：查询订阅确认是否已经收到。 */
uint8_t OneNet_IsSubscribeReady(void);

/* 网络任务调用：查询当前 MQTT 会话是否需要重连。 */
uint8_t OneNet_HasSessionError(void);
/* 网络任务重连前调用：清除会话错误状态。 */
void OneNet_ClearSessionError(void);
/* 网络任务/调试调用：读取最近一次 OneNet 状态码。 */
uint8_t OneNet_GetLastStatus(void);
/* 网络任务/调试调用：读取最近一次 CONNACK 返回码。 */
uint8_t OneNet_GetLastConnAckCode(void);
/* 网络任务/调试调用：读取最近一次订阅 topic。 */
const char *OneNet_GetLastSubscribeTopic(void);

#endif /* __ONENET_H__ */
