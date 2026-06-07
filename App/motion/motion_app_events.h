#ifndef MOTION_APP_EVENTS_H
#define MOTION_APP_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 运动任务调用：排队等待网络任务上报 test 与 confidence。 */
void MotionEvents_QueueTestConfidence(int32_t test_value, float confidence);
/* 网络任务调用：读取待上报 test 与 confidence，成功返回 1。 */
uint8_t MotionEvents_PeekTestConfidence(int32_t *test_value, float *confidence);
/* 网络任务调用：确认上报成功后清除当前 test 与 confidence。 */
void MotionEvents_ClearTestConfidenceIfCurrent(int32_t test_value);

/* 运动任务调用：生成 action_kind 的交替低位，避免云端过滤重复值。 */
int32_t MotionEvents_EncodeActionKind(int32_t test_value);
/* 运动任务调用：排队等待网络任务上报 action_kind。 */
void MotionEvents_QueueActionKind(int32_t action_kind_value);
/* 网络任务调用：读取待上报 action_kind，成功返回 1。 */
uint8_t MotionEvents_PeekActionKind(int32_t *action_kind_value);
/* 网络任务调用：确认上报成功后清除当前 action_kind。 */
void MotionEvents_ClearActionKindIfCurrent(int32_t action_kind_value);
uint8_t MotionEvents_HasPendingCloudPost(void);

/* 运动任务调用：动作完成后请求屏幕任务刷新训练计划页。 */
void MotionEvents_RequestTrainingPageRefresh(void);
void MotionEvents_RequestTrainingPageRefreshByAction(int32_t action_label);
/* 屏幕任务调用：取走训练计划页刷新请求，成功返回 1。 */
uint8_t MotionEvents_TakeTrainingPageRefresh(void);
uint8_t MotionEvents_TakeTrainingPageRefreshAction(int32_t *action_label);
uint8_t MotionEvents_PeekTrainingPageRefreshAction(int32_t *action_label);
uint8_t MotionEvents_PeekTrainingPageRefreshDirty(int32_t *action_label,
                                                  uint32_t *generation);
void MotionEvents_ClearTrainingPageRefresh(void);
void MotionEvents_ClearTrainingPageRefreshIfCurrent(uint32_t generation);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_APP_EVENTS_H */
