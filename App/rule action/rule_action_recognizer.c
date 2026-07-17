#include "rule_action_recognizer.h"

#include <stddef.h>
#include <string.h>

#define RULE_DISTANCE_INVALID (1000000.0f)

typedef struct
{
  ActionType action;
  float center[RULE_UPPER_AXIS_COUNT];
  float tolerance[RULE_UPPER_AXIS_COUNT];
  float weight[RULE_UPPER_AXIS_COUNT];
} RuleActionTemplate;

static const char * const g_rule_state_names[] = {
  "WAIT_STATICS",
  "WAIT_STATIC",
  "READY",
  "RECORDING",
  "ANALYZE",
  "DONE"
};

static const char * const g_rule_action_names[] = {
  "none",
  "elbow_flex",
  "front_raise",
  "side_raise",
  "shoulder_raise",
  "unknown1",
  "unknown2"
};

static const RuleActionTemplate g_rule_templates[] = {
  {
    ACTION_ELBOW_FLEX,
    {
      RULE_TEMPLATE_ELBOW_YAW_CENTER_DEG,
      RULE_TEMPLATE_ELBOW_PITCH_CENTER_DEG,
      RULE_TEMPLATE_ELBOW_ROLL_CENTER_DEG
    },
    {
      RULE_TEMPLATE_ELBOW_YAW_TOL_DEG,
      RULE_TEMPLATE_ELBOW_PITCH_TOL_DEG,
      RULE_TEMPLATE_ELBOW_ROLL_TOL_DEG
    },
    {1.0f, 1.0f, 1.0f}
  },
  {
    ACTION_FRONT_RAISE,
    {
      RULE_TEMPLATE_FRONT_YAW_CENTER_DEG,
      RULE_TEMPLATE_FRONT_PITCH_CENTER_DEG,
      RULE_TEMPLATE_FRONT_ROLL_CENTER_DEG
    },
    {
      RULE_TEMPLATE_FRONT_YAW_TOL_DEG,
      RULE_TEMPLATE_FRONT_PITCH_TOL_DEG,
      RULE_TEMPLATE_FRONT_ROLL_TOL_DEG
    },
    {1.0f, 1.0f, 1.0f}
  },
  {
    ACTION_SIDE_RAISE,
    {
      RULE_TEMPLATE_SIDE_YAW_CENTER_DEG,
      RULE_TEMPLATE_SIDE_PITCH_CENTER_DEG,
      RULE_TEMPLATE_SIDE_ROLL_CENTER_DEG
    },
    {
      RULE_TEMPLATE_SIDE_YAW_TOL_DEG,
      RULE_TEMPLATE_SIDE_PITCH_TOL_DEG,
      RULE_TEMPLATE_SIDE_ROLL_TOL_DEG
    },
    {1.0f, 1.0f, 1.0f}
  },
  {
    ACTION_SHOULDER_RAISE,
    {
      RULE_TEMPLATE_SHOULDER_YAW_CENTER_DEG,
      RULE_TEMPLATE_SHOULDER_PITCH_CENTER_DEG,
      RULE_TEMPLATE_SHOULDER_ROLL_CENTER_DEG
    },
    {
      RULE_TEMPLATE_SHOULDER_YAW_TOL_DEG,
      RULE_TEMPLATE_SHOULDER_PITCH_TOL_DEG,
      RULE_TEMPLATE_SHOULDER_ROLL_TOL_DEG
    },
    {1.0f, 1.0f, 1.0f}
  }
};

static float s_rule_median_scratch[RULE_MAX_RECORD_FRAMES];

static float rule_absf(float value);
static float rule_maxf(float a, float b);
static float rule_normalize_angle_delta(float value);
static uint32_t rule_elapsed_ms(uint32_t start_ms, uint32_t now_ms);
static float rule_median_values(const float *values, uint16_t count);
static float rule_pose_offset(
  const float pose[RULE_UPPER_AXIS_COUNT],
  const float base[RULE_UPPER_AXIS_COUNT]);
static void rule_reset_result(ActionResult *result);
static void rule_reset_static_window(RuleEngine *eng, uint32_t now_ms);
static void rule_enter_state(RuleEngine *eng, RuleState state);
static void rule_enter_recover(RuleEngine *eng);
static void rule_enter_wait_static(RuleEngine *eng);
static void rule_update_pose(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms);
static void rule_update_delta(RuleEngine *eng);
static void rule_lock_baseline(RuleEngine *eng);
static void rule_store_pretrigger(RuleEngine *eng);
static uint8_t rule_append_session_sample(
  ActionSession *session,
  const RulePoseSample *sample);
static void rule_begin_recording(RuleEngine *eng);
static float rule_sample_offset(
  const RulePoseSample *sample,
  const float base[RULE_UPPER_AXIS_COUNT]);
static float rule_median_axis(
  const ActionSession *session,
  uint16_t start_index,
  uint16_t end_index,
  uint32_t axis);
static uint8_t rule_find_top_window(
  const RuleEngine *eng,
  uint16_t *out_start_index,
  uint16_t *out_end_index);
static uint8_t rule_side_direction_match(const ActionSession *session);
static float rule_template_distance(
  const ActionSession *session,
  const RuleActionTemplate *tmpl);
static ActionType rule_classify_action(
  const ActionSession *session,
  float *out_best_distance,
  float *out_second_distance);
static void rule_copy_result(
  const ActionSession *session,
  ActionResult *result);
static void rule_analyze_session(RuleEngine *eng);

void RuleConfig_LoadDefault(RuleConfig *cfg)
{
  if (cfg == NULL)
  {
    return;
  }

  cfg->recover_stable_speed_dps = RULE_CFG_RECOVER_STABLE_SPEED_DPS;
  cfg->recover_stable_ms = RULE_CFG_RECOVER_STABLE_MS;
  cfg->static_accept_speed_dps = RULE_CFG_STATIC_ACCEPT_SPEED_DPS;
  cfg->static_break_speed_dps = RULE_CFG_STATIC_BREAK_SPEED_DPS;
  cfg->static_break_confirm_ms = RULE_CFG_STATIC_BREAK_CONFIRM_MS;
  cfg->static_window_ms = RULE_CFG_STATIC_WINDOW_MS;
  cfg->static_min_accept_ratio = RULE_CFG_STATIC_MIN_ACCEPT_RATIO;
  cfg->static_min_accept_frames = RULE_CFG_STATIC_MIN_ACCEPT_FRAMES;
  cfg->start_speed_dps = RULE_CFG_START_SPEED_DPS;
  cfg->start_offset_deg = RULE_CFG_START_OFFSET_DEG;
  cfg->start_confirm_ms = RULE_CFG_START_CONFIRM_MS;
  cfg->pretrigger_ms = RULE_CFG_PRETRIGGER_MS;
  cfg->record_min_ms = RULE_CFG_RECORD_MIN_MS;
  cfg->action_timeout_ms = RULE_CFG_ACTION_TIMEOUT_MS;
  cfg->action_min_peak_offset_deg = RULE_CFG_ACTION_MIN_PEAK_OFFSET_DEG;
  cfg->return_offset_deg = RULE_CFG_RETURN_OFFSET_DEG;
  cfg->return_speed_dps = RULE_CFG_RETURN_SPEED_DPS;
  cfg->return_stable_ms = RULE_CFG_RETURN_STABLE_MS;
  cfg->top_near_max_deg = RULE_CFG_TOP_NEAR_MAX_DEG;
  cfg->top_stable_speed_dps = RULE_CFG_TOP_STABLE_SPEED_DPS;
  cfg->top_min_hold_ms = RULE_CFG_TOP_MIN_HOLD_MS;
}

void RuleEngine_Init(RuleEngine *eng, const RuleConfig *cfg)
{
  RuleConfig default_cfg;

  if (eng == NULL)
  {
    return;
  }

  RuleConfig_LoadDefault(&default_cfg);
  memset(eng, 0, sizeof(*eng));
  eng->cfg = (cfg != NULL) ? (*cfg) : default_cfg;
  eng->state = RULE_STATE_WAIT_STATICS;
  eng->initialized = 1U;
  rule_reset_result(&eng->result);
}

void RuleEngine_ResetSession(RuleEngine *eng)
{
  if (eng == NULL)
  {
    return;
  }

  memset(&eng->session, 0, sizeof(eng->session));
  rule_reset_result(&eng->result);
  eng->pretrigger_head = 0U;
  eng->pretrigger_count = 0U;
  eng->ready_start_timer_active = 0U;
  eng->return_timer_active = 0U;
}

void RuleEngine_ProcessRaw(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms)
{
  uint32_t elapsed_ms;

  if ((eng == NULL) || (raw == NULL))
  {
    return;
  }

  if (eng->initialized == 0U)
  {
    RuleEngine_Init(eng, NULL);
  }

  eng->now_ms = now_ms;
  rule_update_pose(eng, raw, now_ms);
  rule_update_delta(eng);

  if ((eng->state == RULE_STATE_DONE) &&
      (rule_elapsed_ms(eng->state_enter_ms, now_ms) > 0U))
  {
    RuleEngine_ResetSession(eng);
    eng->baseline_valid = 0U;
    rule_enter_recover(eng);
    return;
  }

  switch (eng->state)
  {
    case RULE_STATE_WAIT_STATICS:
      if (eng->motion_axis_speed_dps[AXIS_UPPER_PITCH] <=
          eng->cfg.recover_stable_speed_dps)
      {
        if (eng->recover_timer_active == 0U)
        {
          eng->recover_timer_active = 1U;
          eng->recover_stable_start_ms = now_ms;
        }
        else if (rule_elapsed_ms(eng->recover_stable_start_ms, now_ms) >=
                 eng->cfg.recover_stable_ms)
        {
          eng->recover_timer_active = 0U;
          rule_enter_wait_static(eng);
        }
      }
      else
      {
        eng->recover_timer_active = 0U;
      }
      break;

    case RULE_STATE_WAIT_STATIC:
      if (eng->static_total_frames < 0xFFFFU)
      {
        eng->static_total_frames++;
      }

      if (eng->motion_speed_dps <= eng->cfg.static_accept_speed_dps)
      {
        uint32_t axis;
        uint16_t baseline_index = eng->static_baseline_frame_count;

        if (eng->static_accept_frames < 0xFFFFU)
        {
          eng->static_accept_frames++;
        }
        for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
        {
          eng->static_sum[axis] += eng->pose[axis];
        }
        if (baseline_index < RULE_STATIC_BASELINE_MAX_FRAMES)
        {
          for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
          {
            eng->static_baseline_frames[baseline_index][axis] =
              eng->pose[axis];
          }
          eng->static_baseline_frame_count++;
        }
      }

      if (eng->motion_speed_dps >= eng->cfg.static_break_speed_dps)
      {
        if (eng->static_break_timer_active == 0U)
        {
          eng->static_break_timer_active = 1U;
          eng->static_break_start_ms = now_ms;
        }
        else if (rule_elapsed_ms(eng->static_break_start_ms, now_ms) >=
                 eng->cfg.static_break_confirm_ms)
        {
          rule_enter_recover(eng);
          break;
        }
      }
      else
      {
        eng->static_break_timer_active = 0U;
      }

      elapsed_ms = rule_elapsed_ms(eng->static_window_start_ms, now_ms);
      if (elapsed_ms >= eng->cfg.static_window_ms)
      {
        float accept_ratio = 0.0f;

        if (eng->static_total_frames > 0U)
        {
          accept_ratio =
            (float)eng->static_accept_frames / (float)eng->static_total_frames;
        }

        if ((eng->static_accept_frames >= eng->cfg.static_min_accept_frames) &&
            (accept_ratio >= eng->cfg.static_min_accept_ratio))
        {
          rule_lock_baseline(eng);
          rule_enter_state(eng, RULE_STATE_READY);
        }
        else
        {
          rule_reset_static_window(eng, now_ms);
        }
      }
      break;

    case RULE_STATE_READY:
      rule_store_pretrigger(eng);

      if ((eng->motion_speed_dps >= eng->cfg.start_speed_dps) &&
          (eng->pose_offset_deg >= eng->cfg.start_offset_deg))
      {
        if (eng->ready_start_timer_active == 0U)
        {
          eng->ready_start_timer_active = 1U;
          eng->ready_start_ms = now_ms;
        }
        else if (rule_elapsed_ms(eng->ready_start_ms, now_ms) >=
                 eng->cfg.start_confirm_ms)
        {
          rule_begin_recording(eng);
        }
      }
      else
      {
        eng->ready_start_timer_active = 0U;
      }
      break;

    case RULE_STATE_RECORDING:
      {
        RulePoseSample sample;

        sample.ts_ms = now_ms;
        sample.speed_dps = eng->motion_speed_dps;
        memcpy(sample.pose, eng->pose, sizeof(sample.pose));

        if (rule_append_session_sample(&eng->session, &sample) == 0U)
        {
          eng->session.timed_out = 1U;
          eng->session.end_ms = now_ms;
          rule_enter_state(eng, RULE_STATE_ANALYZE);
          break;
        }

        eng->session.return_error_deg = eng->pose_offset_deg;
        eng->session.max_offset_deg = rule_maxf(
          eng->session.max_offset_deg,
          eng->pose_offset_deg);
        elapsed_ms = rule_elapsed_ms(eng->session.start_ms, now_ms);

        if ((elapsed_ms >= eng->cfg.record_min_ms) &&
            (eng->session.max_offset_deg >= eng->cfg.action_min_peak_offset_deg) &&
            (eng->pose_offset_deg <= eng->cfg.return_offset_deg) &&
            (eng->motion_speed_dps <= eng->cfg.return_speed_dps))
        {
          if (eng->return_timer_active == 0U)
          {
            eng->return_timer_active = 1U;
            eng->return_stable_start_ms = now_ms;
          }
          else if (rule_elapsed_ms(eng->return_stable_start_ms, now_ms) >=
                   eng->cfg.return_stable_ms)
          {
            eng->session.returned_to_static = 1U;
            eng->session.end_ms = eng->return_stable_start_ms;
            rule_enter_state(eng, RULE_STATE_ANALYZE);
            break;
          }
        }
        else
        {
          eng->return_timer_active = 0U;
        }

        if (elapsed_ms >= eng->cfg.action_timeout_ms)
        {
          eng->session.timed_out = 1U;
          eng->session.end_ms = now_ms;
          rule_enter_state(eng, RULE_STATE_ANALYZE);
        }
      }
      break;

    case RULE_STATE_ANALYZE:
      rule_analyze_session(eng);
      rule_enter_state(eng, RULE_STATE_DONE);
      break;

    case RULE_STATE_DONE:
    default:
      break;
  }
}

const ActionResult* RuleEngine_GetResult(const RuleEngine *eng)
{
  if (eng == NULL)
  {
    return NULL;
  }

  return &eng->result;
}

const ActionSession* RuleEngine_GetSession(const RuleEngine *eng)
{
  if (eng == NULL)
  {
    return NULL;
  }

  return &eng->session;
}

const char* Rule_StateName(RuleState state)
{
  if ((uint32_t)state <
      (sizeof(g_rule_state_names) / sizeof(g_rule_state_names[0])))
  {
    return g_rule_state_names[(uint32_t)state];
  }

  return "UNKNOWN_STATE";
}

const char* Rule_ActionName(ActionType action)
{
  if ((uint32_t)action <
      (sizeof(g_rule_action_names) / sizeof(g_rule_action_names[0])))
  {
    return g_rule_action_names[(uint32_t)action];
  }

  return "unknown2";
}

static float rule_absf(float value)
{
  return (value >= 0.0f) ? value : -value;
}

static float rule_maxf(float a, float b)
{
  return (a >= b) ? a : b;
}

static float rule_normalize_angle_delta(float value)
{
  while (value > 180.0f)
  {
    value -= 360.0f;
  }
  while (value < -180.0f)
  {
    value += 360.0f;
  }

  return value;
}

static uint32_t rule_elapsed_ms(uint32_t start_ms, uint32_t now_ms)
{
  return now_ms - start_ms;
}

static float rule_median_values(const float *values, uint16_t count)
{
  uint16_t i;

  if ((values == NULL) || (count == 0U))
  {
    return 0.0f;
  }

  if (count > RULE_MAX_RECORD_FRAMES)
  {
    count = RULE_MAX_RECORD_FRAMES;
  }

  for (i = 0U; i < count; i++)
  {
    s_rule_median_scratch[i] = values[i];
  }

  for (i = 1U; i < count; i++)
  {
    float value = s_rule_median_scratch[i];
    uint16_t j = i;

    while ((j > 0U) && (s_rule_median_scratch[j - 1U] > value))
    {
      s_rule_median_scratch[j] = s_rule_median_scratch[j - 1U];
      j--;
    }
    s_rule_median_scratch[j] = value;
  }

  if ((count & 1U) != 0U)
  {
    return s_rule_median_scratch[count / 2U];
  }

  return (s_rule_median_scratch[(count / 2U) - 1U] +
          s_rule_median_scratch[count / 2U]) * 0.5f;
}

static float rule_pose_offset(
  const float pose[RULE_UPPER_AXIS_COUNT],
  const float base[RULE_UPPER_AXIS_COUNT])
{
  uint32_t axis;
  float offset = 0.0f;

  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    offset = rule_maxf(offset, rule_absf(pose[axis] - base[axis]));
  }

  return offset;
}

static void rule_reset_result(ActionResult *result)
{
  if (result == NULL)
  {
    return;
  }

  memset(result, 0, sizeof(*result));
  result->action = ACTION_NONE;
  result->best_distance = RULE_DISTANCE_INVALID;
  result->second_distance = RULE_DISTANCE_INVALID;
}

static void rule_reset_static_window(RuleEngine *eng, uint32_t now_ms)
{
  if (eng == NULL)
  {
    return;
  }

  eng->static_window_start_ms = now_ms;
  eng->static_total_frames = 0U;
  eng->static_accept_frames = 0U;
  eng->static_baseline_frame_count = 0U;
  eng->static_break_timer_active = 0U;
  memset(eng->static_sum, 0, sizeof(eng->static_sum));
}

static void rule_enter_state(RuleEngine *eng, RuleState state)
{
  eng->state = state;
  eng->state_enter_ms = eng->now_ms;
}

static void rule_enter_recover(RuleEngine *eng)
{
  eng->baseline_valid = 0U;
  eng->recover_timer_active = 0U;
  rule_reset_static_window(eng, eng->now_ms);
  rule_enter_state(eng, RULE_STATE_WAIT_STATICS);
}

static void rule_enter_wait_static(RuleEngine *eng)
{
  eng->baseline_valid = 0U;
  eng->recover_timer_active = 0U;
  rule_reset_static_window(eng, eng->now_ms);
  rule_enter_state(eng, RULE_STATE_WAIT_STATIC);
}

static void rule_update_pose(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms)
{
  uint32_t axis;
  uint32_t dt_ms;
  float max_speed = 0.0f;

  if (eng->has_prev_raw == 0U)
  {
    for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
    {
      eng->raw[axis] = raw[axis];
      eng->prev_raw[axis] = raw[axis];
      eng->pose[axis] = raw[axis];
      eng->motion_axis_speed_dps[axis] = 0.0f;
    }
    eng->prev_ms = now_ms;
    eng->motion_speed_dps = 0.0f;
    eng->has_prev_raw = 1U;
    return;
  }

  dt_ms = rule_elapsed_ms(eng->prev_ms, now_ms);
  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    float frame_delta = rule_normalize_angle_delta(raw[axis] - eng->prev_raw[axis]);
    float axis_speed = 0.0f;

    eng->raw[axis] = raw[axis];
    eng->pose[axis] += frame_delta;
    eng->prev_raw[axis] = raw[axis];

    if (dt_ms > 0U)
    {
      axis_speed = rule_absf(frame_delta) * 1000.0f / (float)dt_ms;
    }
    eng->motion_axis_speed_dps[axis] = axis_speed;
    max_speed = rule_maxf(max_speed, axis_speed);
  }

  eng->prev_ms = now_ms;
  eng->motion_speed_dps = max_speed;
}

static void rule_update_delta(RuleEngine *eng)
{
  uint32_t axis;

  if (eng->baseline_valid == 0U)
  {
    memset(eng->delta, 0, sizeof(eng->delta));
    eng->pose_offset_deg = 0.0f;
    return;
  }

  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    eng->delta[axis] = eng->pose[axis] - eng->base_pose[axis];
  }
  eng->pose_offset_deg = rule_pose_offset(eng->pose, eng->base_pose);
}

static void rule_lock_baseline(RuleEngine *eng)
{
  uint32_t axis;

  if ((eng == NULL) || (eng->static_accept_frames == 0U) ||
      (eng->static_baseline_frame_count == 0U))
  {
    return;
  }

  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    uint16_t index;

    for (index = 0U; index < eng->static_baseline_frame_count; index++)
    {
      s_rule_median_scratch[index] =
        eng->static_baseline_frames[index][axis];
    }
    eng->base_pose[axis] = rule_median_values(
      s_rule_median_scratch,
      eng->static_baseline_frame_count);
  }
  eng->baseline_valid = 1U;
  eng->pose_offset_deg = rule_pose_offset(eng->pose, eng->base_pose);
  eng->pretrigger_head = 0U;
  eng->pretrigger_count = 0U;
}

static void rule_store_pretrigger(RuleEngine *eng)
{
  RulePoseSample *sample;

  sample = &eng->pretrigger[eng->pretrigger_head];
  sample->ts_ms = eng->now_ms;
  sample->speed_dps = eng->motion_speed_dps;
  memcpy(sample->pose, eng->pose, sizeof(sample->pose));

  eng->pretrigger_head =
    (uint16_t)((eng->pretrigger_head + 1U) % RULE_PRETRIGGER_MAX_FRAMES);
  if (eng->pretrigger_count < RULE_PRETRIGGER_MAX_FRAMES)
  {
    eng->pretrigger_count++;
  }
}

static uint8_t rule_append_session_sample(
  ActionSession *session,
  const RulePoseSample *sample)
{
  if ((session == NULL) || (sample == NULL) ||
      (session->frame_count >= RULE_MAX_RECORD_FRAMES))
  {
    return 0U;
  }

  session->frames[session->frame_count] = *sample;
  session->frame_count++;
  return 1U;
}

static void rule_begin_recording(RuleEngine *eng)
{
  uint16_t index;
  uint16_t oldest;

  memset(&eng->session, 0, sizeof(eng->session));
  rule_reset_result(&eng->result);
  eng->session.active = 1U;
  memcpy(eng->session.base_pose, eng->base_pose, sizeof(eng->session.base_pose));

  oldest = (uint16_t)(
    (eng->pretrigger_head + RULE_PRETRIGGER_MAX_FRAMES -
     eng->pretrigger_count) % RULE_PRETRIGGER_MAX_FRAMES);

  for (index = 0U; index < eng->pretrigger_count; index++)
  {
    uint16_t sample_index =
      (uint16_t)((oldest + index) % RULE_PRETRIGGER_MAX_FRAMES);
    const RulePoseSample *sample = &eng->pretrigger[sample_index];

    if (rule_elapsed_ms(sample->ts_ms, eng->now_ms) <= eng->cfg.pretrigger_ms)
    {
      (void)rule_append_session_sample(&eng->session, sample);
    }
  }

  eng->session.start_ms = eng->ready_start_ms;

  eng->ready_start_timer_active = 0U;
  eng->return_timer_active = 0U;
  rule_enter_state(eng, RULE_STATE_RECORDING);
}

static float rule_sample_offset(
  const RulePoseSample *sample,
  const float base[RULE_UPPER_AXIS_COUNT])
{
  return rule_pose_offset(sample->pose, base);
}

static float rule_median_axis(
  const ActionSession *session,
  uint16_t start_index,
  uint16_t end_index,
  uint32_t axis)
{
  uint16_t count;
  uint16_t i;

  count = (uint16_t)(end_index - start_index + 1U);
  for (i = 0U; i < count; i++)
  {
    s_rule_median_scratch[i] =
      session->frames[(uint16_t)(start_index + i)].pose[axis];
  }

  for (i = 1U; i < count; i++)
  {
    float value = s_rule_median_scratch[i];
    uint16_t j = i;

    while ((j > 0U) && (s_rule_median_scratch[j - 1U] > value))
    {
      s_rule_median_scratch[j] = s_rule_median_scratch[j - 1U];
      j--;
    }
    s_rule_median_scratch[j] = value;
  }

  if ((count & 1U) != 0U)
  {
    return s_rule_median_scratch[count / 2U];
  }

  return (s_rule_median_scratch[(count / 2U) - 1U] +
          s_rule_median_scratch[count / 2U]) * 0.5f;
}

static uint8_t rule_find_top_window(
  const RuleEngine *eng,
  uint16_t *out_start_index,
  uint16_t *out_end_index)
{
  const ActionSession *session = &eng->session;
  float max_offset = 0.0f;
  uint16_t index;
  uint16_t run_start = 0U;
  uint16_t best_start = 0U;
  uint16_t best_end = 0U;
  uint32_t best_duration = 0U;
  uint8_t in_run = 0U;

  if ((session->frame_count == 0U) ||
      (out_start_index == NULL) || (out_end_index == NULL))
  {
    return 0U;
  }

  for (index = 0U; index < session->frame_count; index++)
  {
    max_offset = rule_maxf(
      max_offset,
      rule_sample_offset(&session->frames[index], session->base_pose));
  }

  for (index = 0U; index < session->frame_count; index++)
  {
    float offset =
      rule_sample_offset(&session->frames[index], session->base_pose);
    uint8_t is_top =
      ((offset >= (max_offset - eng->cfg.top_near_max_deg)) &&
       (session->frames[index].speed_dps <= eng->cfg.top_stable_speed_dps)) ?
      1U : 0U;

    if ((is_top != 0U) && (in_run == 0U))
    {
      run_start = index;
      in_run = 1U;
    }

    if ((in_run != 0U) &&
        ((is_top == 0U) || (index == (uint16_t)(session->frame_count - 1U))))
    {
      uint16_t run_end = (is_top != 0U) ? index : (uint16_t)(index - 1U);
      uint32_t duration = rule_elapsed_ms(
        session->frames[run_start].ts_ms,
        session->frames[run_end].ts_ms);

      if (duration > best_duration)
      {
        best_duration = duration;
        best_start = run_start;
        best_end = run_end;
      }
      in_run = 0U;
    }
  }

  if (best_duration < eng->cfg.top_min_hold_ms)
  {
    return 0U;
  }

  *out_start_index = best_start;
  *out_end_index = best_end;
  return 1U;
}

static uint8_t rule_side_direction_match(const ActionSession *session)
{
  float abs_yaw;
  float abs_pitch;
  float abs_roll;
  float total_sq;
  float pitch_ratio_sq;

  if ((session == NULL) || (session->top_valid == 0U))
  {
    return 0U;
  }

  abs_yaw = rule_absf(session->top_delta[AXIS_UPPER_YAW]);
  abs_pitch = rule_absf(session->top_delta[AXIS_UPPER_PITCH]);
  abs_roll = rule_absf(session->top_delta[AXIS_UPPER_ROLL]);

  if ((abs_pitch < RULE_SIDE_DIRECTION_MIN_PITCH_DELTA_DEG) ||
      (abs_pitch > RULE_SIDE_DIRECTION_MAX_PITCH_DELTA_DEG))
  {
    return 0U;
  }

  total_sq =
    (abs_yaw * abs_yaw) +
    (abs_pitch * abs_pitch) +
    (abs_roll * abs_roll);
  if (total_sq <= 0.0f)
  {
    return 0U;
  }

  pitch_ratio_sq =
    RULE_SIDE_DIRECTION_MIN_PITCH_RATIO *
    RULE_SIDE_DIRECTION_MIN_PITCH_RATIO;
  if ((abs_pitch * abs_pitch) < (pitch_ratio_sq * total_sq))
  {
    return 0U;
  }

  if ((abs_yaw + abs_roll) >
      (RULE_SIDE_DIRECTION_MAX_YR_RATIO * abs_pitch))
  {
    return 0U;
  }

  return 1U;
}

static float rule_template_distance(
  const ActionSession *session,
  const RuleActionTemplate *tmpl)
{
  uint32_t axis;
  float weighted_distance = 0.0f;
  float weight_sum = 0.0f;

  if ((tmpl->action == ACTION_SIDE_RAISE) &&
      (rule_side_direction_match(session) == 0U))
  {
    return RULE_DISTANCE_INVALID;
  }

  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    float tolerance = tmpl->tolerance[axis];

    if (tolerance <= 0.0f)
    {
      continue;
    }

    weighted_distance += tmpl->weight[axis] *
      rule_absf(rule_absf(session->top_delta[axis]) - tmpl->center[axis]) /
      tolerance;
    weight_sum += tmpl->weight[axis];
  }

  if (weight_sum <= 0.0f)
  {
    return RULE_DISTANCE_INVALID;
  }

  return weighted_distance / weight_sum;
}

static ActionType rule_classify_action(
  const ActionSession *session,
  float *out_best_distance,
  float *out_second_distance)
{
  uint32_t index;
  float front_distance = RULE_DISTANCE_INVALID;
  float side_distance = RULE_DISTANCE_INVALID;
  uint8_t is_side_direction;

  for (index = 0U;
       index < (sizeof(g_rule_templates) / sizeof(g_rule_templates[0]));
       index++)
  {
    if (g_rule_templates[index].action == ACTION_FRONT_RAISE)
    {
      front_distance = rule_template_distance(session, &g_rule_templates[index]);
    }
    else if (g_rule_templates[index].action == ACTION_SIDE_RAISE)
    {
      side_distance = rule_template_distance(session, &g_rule_templates[index]);
    }
  }

  is_side_direction = rule_side_direction_match(session);
  if (out_best_distance != NULL)
  {
    *out_best_distance = is_side_direction ? side_distance : front_distance;
  }
  if (out_second_distance != NULL)
  {
    *out_second_distance = is_side_direction ? front_distance : side_distance;
  }

  return (is_side_direction != 0U) ? ACTION_SIDE_RAISE : ACTION_FRONT_RAISE;
}

static void rule_copy_result(
  const ActionSession *session,
  ActionResult *result)
{
  result->valid = 1U;
  result->returned_to_static = session->returned_to_static;
  result->timed_out = session->timed_out;
  result->rise_time_ms = session->rise_time_ms;
  result->hold_time_ms = session->hold_time_ms;
  result->fall_time_ms = session->fall_time_ms;
  result->total_time_ms = session->total_time_ms;
  result->return_error_deg = session->return_error_deg;
  memcpy(result->top_pose, session->top_pose, sizeof(result->top_pose));
  memcpy(result->top_delta, session->top_delta, sizeof(result->top_delta));
}

static void rule_analyze_session(RuleEngine *eng)
{
  ActionSession *session = &eng->session;
  uint16_t top_start_index = 0U;
  uint16_t top_end_index = 0U;
  uint16_t index;
  uint32_t axis;

  rule_reset_result(&eng->result);
  session->active = 0U;
  session->total_time_ms = rule_elapsed_ms(session->start_ms, session->end_ms);

  if (session->frame_count < 3U)
  {
    eng->result.valid = 1U;
    eng->result.action = ACTION_UNKNOWN2;
    return;
  }

  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    float first_delta =
      session->frames[0].pose[axis] - session->base_pose[axis];

    session->max_delta[axis] = first_delta;
    session->min_delta[axis] = first_delta;
  }

  session->max_offset_deg = 0.0f;
  for (index = 0U; index < session->frame_count; index++)
  {
    float offset =
      rule_sample_offset(&session->frames[index], session->base_pose);

    session->max_offset_deg = rule_maxf(session->max_offset_deg, offset);
    for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
    {
      float delta =
        session->frames[index].pose[axis] - session->base_pose[axis];

      if (delta > session->max_delta[axis])
      {
        session->max_delta[axis] = delta;
      }
      if (delta < session->min_delta[axis])
      {
        session->min_delta[axis] = delta;
      }
    }
  }

  if (rule_find_top_window(eng, &top_start_index, &top_end_index) == 0U)
  {
    eng->result.valid = 1U;
    eng->result.action = ACTION_UNKNOWN2;
    return;
  }

  session->top_valid = 1U;
  for (axis = 0U; axis < RULE_UPPER_AXIS_COUNT; axis++)
  {
    session->top_pose[axis] =
      rule_median_axis(session, top_start_index, top_end_index, axis);
    session->top_delta[axis] =
      session->top_pose[axis] - session->base_pose[axis];
    session->rise_direction[axis] =
      (session->top_delta[axis] > 0.0f) ? 1.0f :
      ((session->top_delta[axis] < 0.0f) ? -1.0f : 0.0f);
  }

  session->rise_time_ms = rule_elapsed_ms(
    session->start_ms,
    session->frames[top_start_index].ts_ms);
  session->hold_time_ms = rule_elapsed_ms(
    session->frames[top_start_index].ts_ms,
    session->frames[top_end_index].ts_ms);
  session->fall_time_ms = rule_elapsed_ms(
    session->frames[top_end_index].ts_ms,
    session->end_ms);

  rule_copy_result(session, &eng->result);

  if (session->max_offset_deg < eng->cfg.action_min_peak_offset_deg)
  {
    eng->result.action = ACTION_UNKNOWN2;
    return;
  }

  eng->result.action = rule_classify_action(
    session,
    &eng->result.best_distance,
    &eng->result.second_distance);
}
