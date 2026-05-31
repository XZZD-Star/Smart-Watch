#ifndef RULE_ACTION_RECOGNIZER_H
#define RULE_ACTION_RECOGNIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RULE_TEMPLATE_DEBUG_MAX_COUNT      (8U)
#define RULE_DEFAULT_START_ENERGY_TH       (40.0f)
#define RULE_DEFAULT_START_CONFIRM_FRAMES  (4U)
#define RULE_DEFAULT_PEAK_EXIT_MIN_HOLD_MS (800U)
#define RULE_DEFAULT_PEAK_EXIT_CONFIRM_FRAMES (2U)
#define RULE_DEFAULT_PEAK_HOLD_MIN_MS      (350U)
#define RULE_DEFAULT_PEAK_HOLD_IDEAL_MIN_MS (600U)
#define RULE_DEFAULT_PEAK_HOLD_IDEAL_MAX_MS (1000U)
#define RULE_DEFAULT_PEAK_HOLD_MAX_MS      (1400U)

typedef enum {
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
  RULE_STATE_STATIC_WAIT = 0,
  RULE_STATE_READY,
  RULE_STATE_ACTION_RISING,
  RULE_STATE_PEAK_HOLD,
  RULE_STATE_ACTION_FALLING,
  RULE_STATE_ACTION_DONE
} RuleState;

typedef enum
{
  ACTION_UNKNOWN = 0,
  ACTION_ELBOW_FLEX,
  ACTION_FRONT_RAISE,
  ACTION_SIDE_RAISE,
  ACTION_SHOULDER_RAISE
} ActionType;

typedef enum
{
  RULE_GRADE_FAIL = 0,
  RULE_GRADE_PASS,
  RULE_GRADE_GOOD,
  RULE_GRADE_EXCELLENT
} RuleGrade;

typedef struct
{
  const char *name;
  ActionType action_type;
  AxisIndex main_axis;
  AxisIndex sub_axis;
  AxisIndex alt_main_axis;

  float amp_min[AXIS_COUNT];
  float amp_max[AXIS_COUNT];

  uint8_t peak_range_enabled[AXIS_COUNT];
  float peak_mean_min[AXIS_COUNT];
  float peak_mean_max[AXIS_COUNT];

  float amp_ref[AXIS_COUNT];
  float peak_mean_ref[AXIS_COUNT];

  float amp_weight[AXIS_COUNT];
  float peak_weight[AXIS_COUNT];

  float amp_min_required;
  float amp_ideal_min;
  float amp_ideal_max;

  uint32_t peak_hold_min_ms;
  uint32_t peak_hold_ideal_min_ms;
  uint32_t peak_hold_ideal_max_ms;
  uint32_t peak_hold_max_ms;
} ActionTemplate;

typedef struct
{
  uint8_t active;
  uint8_t has_static_start;
  uint8_t has_rising_phase;
  uint8_t has_peak_hold;
  uint8_t has_falling_phase;
  uint8_t returned_to_static;

  uint32_t frame_count;
  uint32_t peak_count;

  uint32_t start_ms;
  uint32_t rise_start_ms;
  uint32_t peak_enter_ms;
  uint32_t fall_start_ms;
  uint32_t end_ms;

  uint32_t rise_time_ms;
  uint32_t peak_hold_ms;
  uint32_t fall_time_ms;
  uint32_t total_time_ms;

  AxisIndex dominant_axis;
  float dominant_amp;

  float base_mean[AXIS_COUNT];
  float last_raw[AXIS_COUNT];
  float last_delta[AXIS_COUNT];

  float delta_max[AXIS_COUNT];
  float delta_min[AXIS_COUNT];
  float amp[AXIS_COUNT];
  float amp_abs[AXIS_COUNT];

  float peak_sum[AXIS_COUNT];
  float peak_mean[AXIS_COUNT];
} ActionSession;

typedef struct
{
  const char *name;
  ActionType action_type;
  uint8_t filter_passed;
  float distance_score;
  float amp_distance;
  float peak_distance;
} RuleTemplateDebug;

typedef struct
{
  ActionType action;
  const char *matched_template_name;
  const ActionTemplate *matched_template;
  uint32_t matched_template_index;

  uint16_t score;
  uint16_t completeness_score;
  uint16_t amplitude_score;
  uint16_t peak_hold_score;

  uint8_t valid;
  uint8_t complete;
  uint8_t timed_out;
  RuleGrade grade;

  AxisIndex primary_axis;
  AxisIndex secondary_axis;
  AxisIndex alt_primary_axis;
  float primary_axis_amp;

  float match_score;
  uint32_t peak_hold_ms;
  uint32_t total_time_ms;

  float amp[AXIS_COUNT];
  float peak_mean[AXIS_COUNT];

  uint32_t template_debug_count;
  RuleTemplateDebug template_debug[RULE_TEMPLATE_DEBUG_MAX_COUNT];
} ActionResult;

typedef struct
{
  float static_energy_th;
  uint16_t n_static_frames;
  float start_energy_th;
  uint16_t start_confirm_frames;

  float peak_enter_amp_th;
  float peak_stable_delta_th;
  uint16_t peak_stable_frames;
  float peak_exit_drop_th;
  uint32_t peak_exit_min_hold_ms;
  uint16_t peak_exit_confirm_frames;

  float return_energy_th;
  float return_axis_th;
  uint16_t return_stable_frames;

  uint32_t action_timeout_ms;
} RuleConfig;

typedef struct
{
  uint8_t initialized;
  uint8_t baseline_valid;
  uint8_t has_prev_raw;

  RuleState state;

  uint16_t static_frame_count;
  uint16_t start_confirm_count;
  uint16_t peak_stable_count;
  uint16_t peak_exit_confirm_count;
  uint16_t return_stable_count;

  uint32_t now_ms;
  float motion_energy;

  float raw[AXIS_COUNT];
  float base_mean[AXIS_COUNT];
  float delta[AXIS_COUNT];
  float prev_delta[AXIS_COUNT];
  float static_sum[AXIS_COUNT];

  const ActionTemplate *templates;
  uint32_t template_count;

  RuleConfig cfg;
  ActionSession session;
  ActionResult result;
} RuleEngine;

/* 载入规则识别默认参数，任务初始化时调用。 */
void RuleConfig_LoadDefault(RuleConfig *cfg);

/* 初始化规则识别引擎，运动任务启动时调用。 */
void RuleEngine_Init(RuleEngine *eng, const RuleConfig *cfg);

/* 清空当前动作会话，start/clear 或 AI 重启后调用。 */
void RuleEngine_ResetSession(RuleEngine *eng);

/* 设置动作模板表，通常使用默认模板。 */
void RuleEngine_SetTemplates(
  RuleEngine *eng,
  const ActionTemplate *templates,
  uint32_t template_count);

/* 输入一帧姿态数据并推进规则状态机，非 ISR 调用。 */
void RuleEngine_Update(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  float motion_energy,
  uint32_t now_ms);

/* 使用原始姿态数据完成能量计算和状态机更新，非 ISR 调用。 */
void RuleEngine_ProcessRaw(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms);

/* 计算当前帧相对上一帧的运动能量。 */
float Rule_ComputeMotionEnergy(const RuleEngine *eng, const float raw[AXIS_COUNT]);

/* 更新动作幅度统计，用于动作质量评分。 */
void Rule_UpdateAmplitudeStats(ActionSession *session, const float delta[AXIS_COUNT]);

/* 更新峰值保持统计，用于动作质量评分。 */
void Rule_UpdatePeakStats(ActionSession *session, const float delta[AXIS_COUNT]);

/* 判断状态机是否进入峰值保持阶段。 */
uint8_t Rule_ShouldEnterPeakHold(RuleEngine *eng);

/* 判断状态机是否退出峰值保持阶段。 */
uint8_t Rule_ShouldExitPeakHold(RuleEngine *eng);

/* 判断动作是否已经回到初始姿态附近。 */
uint8_t Rule_IsActionReturned(RuleEngine *eng);

/* 根据会话统计快速过滤不匹配的动作模板。 */
uint8_t Rule_FilterTemplate(
  const ActionSession *session,
  const ActionTemplate *tmpl);

/* 计算动作会话和模板的匹配分数。 */
float Rule_ComputeTemplateMatchScore(
  const ActionSession *session,
  const ActionTemplate *tmpl,
  float *amp_distance,
  float *peak_distance);

/* 从模板表中识别动作类型并输出结果。 */
ActionType Rule_RecognizeAction(
  const ActionSession *session,
  const ActionTemplate *templates,
  uint32_t template_count,
  ActionResult *out_result);

/* 计算动作完整度分数。 */
uint16_t Rule_ScoreCompleteness(const ActionSession *session);

/* 计算动作幅度分数。 */
uint16_t Rule_ScoreAmplitude(
  const ActionSession *session,
  const ActionTemplate *tmpl);

/* 计算峰值保持分数。 */
uint16_t Rule_ScorePeakHold(
  const ActionSession *session,
  const ActionTemplate *tmpl);

/* 汇总规则识别结果，供运动任务输出。 */
void Rule_EvaluateResult(
  const RuleConfig *cfg,
  const ActionSession *session,
  ActionResult *out_result);

/* 获取内置动作模板表。 */
const ActionTemplate* Rule_GetDefaultTemplates(uint32_t *count);

/* 获取最近一次规则识别结果。 */
const ActionResult* RuleEngine_GetResult(const RuleEngine *eng);

/* 获取当前规则识别会话状态。 */
const ActionSession* RuleEngine_GetSession(const RuleEngine *eng);

/* 调试输出：规则状态名。 */
const char* Rule_StateName(RuleState state);

/* 调试输出：动作名。 */
const char* Rule_ActionName(ActionType action);

/* 调试输出：评分等级名。 */
const char* Rule_GradeName(RuleGrade grade);

/* 调试输出：姿态轴名。 */
const char* Rule_AxisName(AxisIndex axis);

#ifdef __cplusplus
}
#endif

#endif /* RULE_ACTION_RECOGNIZER_H */
