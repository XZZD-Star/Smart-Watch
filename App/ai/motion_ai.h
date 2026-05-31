#ifndef MOTION_AI_H
#define MOTION_AI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "app_x-cube-ai.h"
#include "motion_frame.h"

#define MOTION_AI_FEATURE_COUNT          APP_X_CUBE_AI_INPUT_FEATURES
#define MOTION_AI_WINDOW_FRAMES          APP_X_CUBE_AI_INPUT_FRAMES
#define MOTION_AI_CLASS_COUNT            APP_X_CUBE_AI_OUTPUT_CLASSES

#define MOTION_AI_BASELINE_TARGET_FRAMES (12U)
#define MOTION_AI_BASELINE_MAX_FRAMES    (15U)
#define MOTION_AI_INFER_STRIDE           (5U)
#define MOTION_AI_SMOOTHING_WINDOW       (4U)
#define MOTION_AI_SINGLE_TEST_MODE_DEFAULT (0U)

#define MOTION_AI_P_KNOWN_TH             (0.75f)
#define MOTION_AI_E_REST_TH              (0.0f)
#define MOTION_AI_ABNORMAL_LATCH_FRAMES  (10U)

#if (MOTION_AI_BASELINE_TARGET_FRAMES > MOTION_AI_BASELINE_MAX_FRAMES)
#error "MOTION_AI_BASELINE_TARGET_FRAMES must be <= MOTION_AI_BASELINE_MAX_FRAMES"
#endif

typedef enum
{
  MOTION_AI_STATE_STATIC_WAIT = 0,
  MOTION_AI_STATE_READY,
  MOTION_AI_STATE_FILL_WINDOW,
  MOTION_AI_STATE_RUNNING,
  MOTION_AI_STATE_TEST_DONE
} motion_ai_state_t;

typedef enum
{
  MOTION_LABEL_REST = 0,
  MOTION_LABEL_ELBOW_FLEX,
  MOTION_LABEL_FRONT_RAISE,
  MOTION_LABEL_SIDE_RAISE,
  MOTION_LABEL_SHOULDER_RAISE,
  MOTION_LABEL_UNKNOWN
} motion_label_t;

typedef enum
{
  MOTION_FALL_LOCAL_STATE_IDLE = 0,
  MOTION_FALL_LOCAL_STATE_SUDDEN_CHANGE,
  MOTION_FALL_LOCAL_STATE_ABNORMAL_POSTURE,
  MOTION_FALL_LOCAL_STATE_STILL_CONFIRM,
  MOTION_FALL_LOCAL_STATE_DETECTED
} motion_fall_local_state_t;

typedef struct
{
  float frame_delta_energy;
  float posture_offset;
  float static_motion_energy;
  uint16_t state_frames;
} motion_fall_local_debug_t;

typedef struct
{
  motion_ai_state_t ai_state;
  motion_label_t top1_label;
  motion_label_t final_label;
  motion_label_t latest_label;
  float base_mean[MOTION_AI_FEATURE_COUNT];
  float delta[MOTION_AI_FEATURE_COUNT];
  float probs[MOTION_AI_CLASS_COUNT];
  float avg_probs[MOTION_AI_CLASS_COUNT];
  float latest_prob;
  float top1_prob_avg;
  float motion_energy;
  uint8_t abnormal_flag;
  uint8_t smooth_ready;
  uint8_t test_done;
  uint8_t infer_count;
  uint32_t align_fail_total;
  uint32_t align_fail_delta;
  uint16_t baseline_count;
  uint16_t window_count;
  uint8_t fall_local_detected;
  uint8_t fall_local_trigger_enabled;
  motion_fall_local_state_t fall_local_state;
  motion_fall_local_debug_t fall_local_debug;
} motion_ai_result_t;

/* 初始化 AI 识别上下文，任务启动时调用一次。 */
void MotionAi_Init(void);
/* 重置基线、窗口和平滑状态，start/模式切换时调用。 */
void MotionAi_Reset(void);
/* 设置是否单次测试完成后停在 TEST_DONE 状态。 */
void MotionAi_SetSingleTestEnabled(uint8_t enabled);
/* 设置本地跌倒检测是否可以主动触发云端告警。 */
void MotionAi_SetLocalFallTriggerEnabled(uint8_t enabled);
/* 查询本地跌倒检测触发开关。 */
uint8_t MotionAi_IsLocalFallTriggerEnabled(void);
/* 云端 test 下发后，用指定动作和分数驱动演示覆盖路径。 */
void MotionAi_SetDemoOverride(int32_t test_value, int32_t action_id, int32_t score);
/* 清除云端演示覆盖状态。 */
void MotionAi_ClearDemoOverride(void);
/* 消费一帧融合姿态数据并推进 AI 状态机。 */
const motion_ai_result_t* MotionAi_ProcessFusedFrame(const motion_fused_frame_t *frame);
/* 读取最近一次 AI 结果，不推进状态机。 */
const motion_ai_result_t* MotionAi_GetResult(void);
/* 将 AI 状态枚举转成调试输出字符串。 */
const char* MotionAi_StateName(motion_ai_state_t state);
/* 将动作标签枚举转成调试输出字符串。 */
const char* MotionAi_LabelName(motion_label_t label);
/* 将本地跌倒状态枚举转成调试输出字符串。 */
const char* MotionAi_FallLocalStateName(motion_fall_local_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_AI_H */
