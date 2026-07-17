#ifndef RULE_ACTION_RECOGNIZER_H
#define RULE_ACTION_RECOGNIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RULE_UPPER_AXIS_COUNT                    (3U)
#define RULE_MAX_RECORD_FRAMES                   (320U)
#define RULE_PRETRIGGER_MAX_FRAMES               (32U)
#define RULE_STATIC_BASELINE_MAX_FRAMES          (24U)

/* 状态切换阈值：后续根据实测 CSV 统一调整这里。 */
#define RULE_CFG_RECOVER_STABLE_SPEED_DPS        (25.0f)
#define RULE_CFG_RECOVER_STABLE_MS               (1200U)
#define RULE_CFG_STATIC_ACCEPT_SPEED_DPS         (30.0f)
#define RULE_CFG_STATIC_BREAK_SPEED_DPS          (180.0f)
#define RULE_CFG_STATIC_BREAK_CONFIRM_MS         (200U)
#define RULE_CFG_STATIC_WINDOW_MS                (1200U)
#define RULE_CFG_STATIC_MIN_ACCEPT_RATIO         (0.80f)
#define RULE_CFG_STATIC_MIN_ACCEPT_FRAMES        (8U)
#define RULE_CFG_START_SPEED_DPS                 (40.0f)
#define RULE_CFG_START_OFFSET_DEG                (8.0f)
#define RULE_CFG_START_CONFIRM_MS                (180U)
#define RULE_CFG_PRETRIGGER_MS                   (300U)
#define RULE_CFG_RECORD_MIN_MS                   (1800U)
#define RULE_CFG_ACTION_TIMEOUT_MS               (5000U)
#define RULE_CFG_ACTION_MIN_PEAK_OFFSET_DEG      (15.0f)
#define RULE_CFG_RETURN_OFFSET_DEG               (10.0f)
#define RULE_CFG_RETURN_SPEED_DPS                (15.0f)
#define RULE_CFG_RETURN_STABLE_MS                (350U)
#define RULE_CFG_TOP_NEAR_MAX_DEG                (8.0f)
#define RULE_CFG_TOP_STABLE_SPEED_DPS            (12.0f)
#define RULE_CFG_TOP_MIN_HOLD_MS                 (400U)

/* 动作时间范围：先作为宽松硬约束，后续根据数据修改。 */
#define RULE_CFG_RISE_MIN_MS                     (500U)
#define RULE_CFG_RISE_MAX_MS                     (1600U)
#define RULE_CFG_HOLD_MIN_MS                     (500U)
#define RULE_CFG_HOLD_MAX_MS                     (1600U)
#define RULE_CFG_FALL_MIN_MS                     (500U)
#define RULE_CFG_FALL_MAX_MS                     (1600U)
#define RULE_CFG_TOTAL_MIN_MS                    (2000U)
#define RULE_CFG_TOTAL_MAX_MS                    (5000U)

/* 模板使用三轴顶点相对基线的绝对偏移，当前数值仅为初始值。 */
#define RULE_TEMPLATE_ELBOW_YAW_CENTER_DEG       (10.0f)
#define RULE_TEMPLATE_ELBOW_PITCH_CENTER_DEG     (25.0f)
#define RULE_TEMPLATE_ELBOW_ROLL_CENTER_DEG      (15.0f)
#define RULE_TEMPLATE_ELBOW_YAW_TOL_DEG          (20.0f)
#define RULE_TEMPLATE_ELBOW_PITCH_TOL_DEG        (20.0f)
#define RULE_TEMPLATE_ELBOW_ROLL_TOL_DEG         (20.0f)

#define RULE_TEMPLATE_FRONT_YAW_CENTER_DEG       (170.0f)
#define RULE_TEMPLATE_FRONT_PITCH_CENTER_DEG     (85.0f)
#define RULE_TEMPLATE_FRONT_ROLL_CENTER_DEG      (165.0f)
#define RULE_TEMPLATE_FRONT_YAW_TOL_DEG          (55.0f)
#define RULE_TEMPLATE_FRONT_PITCH_TOL_DEG        (15.0f)
#define RULE_TEMPLATE_FRONT_ROLL_TOL_DEG         (55.0f)

#define RULE_TEMPLATE_SIDE_YAW_CENTER_DEG        (50.0f)
#define RULE_TEMPLATE_SIDE_PITCH_CENTER_DEG      (78.0f)
#define RULE_TEMPLATE_SIDE_ROLL_CENTER_DEG       (72.0f)
#define RULE_TEMPLATE_SIDE_YAW_TOL_DEG           (45.0f)
#define RULE_TEMPLATE_SIDE_PITCH_TOL_DEG         (15.0f)
#define RULE_TEMPLATE_SIDE_ROLL_TOL_DEG          (45.0f)

#define RULE_TEMPLATE_SHOULDER_YAW_CENTER_DEG    (35.0f)
#define RULE_TEMPLATE_SHOULDER_PITCH_CENTER_DEG  (145.0f)
#define RULE_TEMPLATE_SHOULDER_ROLL_CENTER_DEG   (35.0f)
#define RULE_TEMPLATE_SHOULDER_YAW_TOL_DEG       (40.0f)
#define RULE_TEMPLATE_SHOULDER_PITCH_TOL_DEG     (35.0f)
#define RULE_TEMPLATE_SHOULDER_ROLL_TOL_DEG      (40.0f)

#define RULE_TEMPLATE_ACCEPT_DISTANCE            (1.0f)
#define RULE_TEMPLATE_MIN_SEPARATION             (0.18f)

/* 侧平举方向门槛：使用“基线到顶点”的三轴偏移方向，而不是原始角度正负。 */
#define RULE_SIDE_DIRECTION_MIN_PITCH_DELTA_DEG  (60.0f)
#define RULE_SIDE_DIRECTION_MAX_PITCH_DELTA_DEG  (110.0f)
#define RULE_SIDE_DIRECTION_MIN_PITCH_RATIO      (0.50f)
#define RULE_SIDE_DIRECTION_MAX_YR_RATIO         (2.05f)

typedef enum
{
  AXIS_UPPER_YAW = 0,
  AXIS_UPPER_PITCH,
  AXIS_UPPER_ROLL,
  AXIS_FORE_YAW,
  AXIS_FORE_PITCH,
  AXIS_FORE_ROLL,
  AXIS_COUNT
} AxisIndex;

typedef enum
{
  RULE_STATE_WAIT_STATICS = 0,
  RULE_STATE_WAIT_STATIC,
  RULE_STATE_READY,
  RULE_STATE_RECORDING,
  RULE_STATE_ANALYZE,
  RULE_STATE_DONE
} RuleState;

typedef enum
{
  ACTION_NONE = 0,
  ACTION_ELBOW_FLEX,
  ACTION_FRONT_RAISE,
  ACTION_SIDE_RAISE,
  ACTION_SHOULDER_RAISE,
  ACTION_UNKNOWN1,
  ACTION_UNKNOWN2
} ActionType;

typedef struct
{
  uint32_t ts_ms;
  float pose[RULE_UPPER_AXIS_COUNT];
  float speed_dps;
} RulePoseSample;

typedef struct
{
  uint8_t active;
  uint8_t returned_to_static;
  uint8_t timed_out;
  uint8_t top_valid;
  uint16_t frame_count;
  uint32_t start_ms;
  uint32_t end_ms;
  uint32_t rise_time_ms;
  uint32_t hold_time_ms;
  uint32_t fall_time_ms;
  uint32_t total_time_ms;
  float max_offset_deg;
  float return_error_deg;
  float base_pose[RULE_UPPER_AXIS_COUNT];
  float top_pose[RULE_UPPER_AXIS_COUNT];
  float top_delta[RULE_UPPER_AXIS_COUNT];
  float max_delta[RULE_UPPER_AXIS_COUNT];
  float min_delta[RULE_UPPER_AXIS_COUNT];
  float rise_direction[RULE_UPPER_AXIS_COUNT];
  RulePoseSample frames[RULE_MAX_RECORD_FRAMES];
} ActionSession;

typedef struct
{
  ActionType action;
  uint8_t valid;
  uint8_t returned_to_static;
  uint8_t timed_out;
  float best_distance;
  float second_distance;
  uint32_t rise_time_ms;
  uint32_t hold_time_ms;
  uint32_t fall_time_ms;
  uint32_t total_time_ms;
  float top_pose[RULE_UPPER_AXIS_COUNT];
  float top_delta[RULE_UPPER_AXIS_COUNT];
  float return_error_deg;
} ActionResult;

typedef struct
{
  float recover_stable_speed_dps;
  uint32_t recover_stable_ms;
  float static_accept_speed_dps;
  float static_break_speed_dps;
  uint32_t static_break_confirm_ms;
  uint32_t static_window_ms;
  float static_min_accept_ratio;
  uint16_t static_min_accept_frames;
  float start_speed_dps;
  float start_offset_deg;
  uint32_t start_confirm_ms;
  uint32_t pretrigger_ms;
  uint32_t record_min_ms;
  uint32_t action_timeout_ms;
  float action_min_peak_offset_deg;
  float return_offset_deg;
  float return_speed_dps;
  uint32_t return_stable_ms;
  float top_near_max_deg;
  float top_stable_speed_dps;
  uint32_t top_min_hold_ms;
} RuleConfig;

typedef struct
{
  uint8_t initialized;
  uint8_t has_prev_raw;
  uint8_t baseline_valid;
  uint8_t recover_timer_active;
  uint8_t static_break_timer_active;
  uint8_t ready_start_timer_active;
  uint8_t return_timer_active;
  RuleState state;
  uint32_t now_ms;
  uint32_t prev_ms;
  uint32_t state_enter_ms;
  uint32_t recover_stable_start_ms;
  uint32_t static_window_start_ms;
  uint32_t static_break_start_ms;
  uint32_t ready_start_ms;
  uint32_t return_stable_start_ms;
  uint16_t static_total_frames;
  uint16_t static_accept_frames;
  uint16_t static_baseline_frame_count;
  float static_sum[RULE_UPPER_AXIS_COUNT];
  float static_baseline_frames
    [RULE_STATIC_BASELINE_MAX_FRAMES][RULE_UPPER_AXIS_COUNT];
  float raw[RULE_UPPER_AXIS_COUNT];
  float prev_raw[RULE_UPPER_AXIS_COUNT];
  float pose[RULE_UPPER_AXIS_COUNT];
  float base_pose[RULE_UPPER_AXIS_COUNT];
  float delta[RULE_UPPER_AXIS_COUNT];
  float motion_axis_speed_dps[RULE_UPPER_AXIS_COUNT];
  float motion_speed_dps;
  float pose_offset_deg;
  RulePoseSample pretrigger[RULE_PRETRIGGER_MAX_FRAMES];
  uint16_t pretrigger_head;
  uint16_t pretrigger_count;
  RuleConfig cfg;
  ActionSession session;
  ActionResult result;
} RuleEngine;

/** @brief 载入当前宏定义中的默认规则参数。 */
void RuleConfig_LoadDefault(RuleConfig *cfg);

/** @brief 初始化规则引擎。 */
void RuleEngine_Init(RuleEngine *eng, const RuleConfig *cfg);

/** @brief 清空当前动作会话和结果。 */
void RuleEngine_ResetSession(RuleEngine *eng);

/** @brief 输入一帧六轴数组；当前规则只读取上臂三轴。 */
void RuleEngine_ProcessRaw(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms);

/** @brief 获取最近一次动作结果。 */
const ActionResult* RuleEngine_GetResult(const RuleEngine *eng);

/** @brief 获取当前动作会话。 */
const ActionSession* RuleEngine_GetSession(const RuleEngine *eng);

/** @brief 获取规则状态名称。 */
const char* Rule_StateName(RuleState state);

/** @brief 获取动作名称。 */
const char* Rule_ActionName(ActionType action);

#ifdef __cplusplus
}
#endif

#endif /* RULE_ACTION_RECOGNIZER_H */
