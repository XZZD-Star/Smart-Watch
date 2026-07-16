#include "motion_ai.h"

#include <math.h>
#include <string.h>

#include "debug_uart7.h"

#define MOTION_AI_STATIC_ENERGY_TH      (6.0f)
#define MOTION_AI_STATIC_CONFIRM_FRAMES (10U)
#define MOTION_AI_START_ENERGY_TH       (40.0f)
#define MOTION_AI_START_CONFIRM_FRAMES  (4U)
#define MOTION_AI_FALL_SUDDEN_ENERGY_TH (120.0f)
#define MOTION_AI_FALL_STILL_ENERGY_TH  (3.0f)
#define MOTION_AI_FALL_POSTURE_OFFSET_TH (28.0f)
#define MOTION_AI_FALL_POSTURE_RESET_TH (12.0f)
#define MOTION_AI_FALL_SUDDEN_CONFIRM_FRAMES (2U)
#define MOTION_AI_FALL_ABNORMAL_CONFIRM_FRAMES (6U)
#define MOTION_AI_FALL_STILL_CONFIRM_FRAMES (8U)
#define MOTION_AI_FALL_DETECTED_CONFIRM_FRAMES (2U)
#define MOTION_AI_FALL_ABNORMAL_TIMEOUT_FRAMES (24U)
#define MOTION_AI_FALL_RECOVER_CONFIRM_FRAMES (8U)

typedef struct
{
  uint8_t initialized;
  uint8_t has_prev_raw;
  uint8_t prob_history_count;
  uint8_t prob_history_next;
  uint16_t fall_state_frames;
  uint16_t fall_sudden_confirm_count;
  uint16_t fall_abnormal_confirm_count;
  uint16_t fall_still_confirm_count;
  uint16_t fall_recover_confirm_count;
  uint16_t frames_since_infer;
  uint16_t start_confirm_count;
  uint32_t abnormal_latch_remaining;
  uint32_t last_align_fail_total;
  float prev_raw[MOTION_AI_FEATURE_COUNT];
  float baseline_cache[MOTION_AI_BASELINE_MAX_FRAMES][MOTION_AI_FEATURE_COUNT];
  float baseline_sum[MOTION_AI_FEATURE_COUNT];
  float window[MOTION_AI_WINDOW_FRAMES][MOTION_AI_FEATURE_COUNT];
  float prob_history[MOTION_AI_SMOOTHING_WINDOW][MOTION_AI_CLASS_COUNT];
  float prob_avg[MOTION_AI_CLASS_COUNT];
  motion_ai_result_t result;
} motion_ai_context_t;

typedef struct
{
  uint8_t pending;
  uint8_t active;
  int32_t test_value;
  int32_t action_id;
  int32_t score;
} motion_ai_demo_override_t;

static motion_ai_context_t g_motion_ai;
static motion_ai_demo_override_t g_motion_ai_demo;
static uint8_t g_motion_ai_single_test_enabled = MOTION_AI_SINGLE_TEST_MODE_DEFAULT;
static uint8_t g_motion_ai_local_fall_trigger_enabled = 0U;

static const char * const g_motion_ai_state_names[] = {
  "STATIC_WAIT",
  "READY",
  "FILL_WINDOW",
  "RUNNING",
  "TEST_DONE"
};

static const char * const g_motion_ai_label_names[] = {
  "rest",
  "elbow_flex",
  "front_raise",
  "side_raise",
  "shoulder_raise",
  "unknown"
};

static const char * const g_motion_ai_fall_local_state_names[] = {
  "IDLE",
  "SUDDEN_CHANGE",
  "ABNORMAL_POSTURE",
  "STILL_CONFIRM",
  "DETECTED"
};

static void motion_ai_reset_smoothing_history(void);
static void motion_ai_reset_context(void);
static void motion_ai_reset_cycle_state(uint8_t clear_prev_raw);
static void motion_ai_frame_to_raw(const motion_fused_frame_t *frame, float raw[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_update_align_fail(const motion_fused_frame_t *frame);
static void motion_ai_update_prev_raw(const float raw[MOTION_AI_FEATURE_COUNT]);
static float motion_ai_compute_gate_motion_energy(const float raw[MOTION_AI_FEATURE_COUNT]);
static float motion_ai_compute_frame_delta_energy(const float raw[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_store_baseline_frame(const float raw[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_finalize_baseline(void);
static void motion_ai_begin_action_session(void);
static uint8_t motion_ai_has_valid_baseline(void);
static void motion_ai_compute_delta(
  const float raw[MOTION_AI_FEATURE_COUNT],
  float delta[MOTION_AI_FEATURE_COUNT]);
static float motion_ai_compute_posture_offset(const float delta[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_update_motion_energy(const float delta[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_reset_local_fall_detection(void);
static void motion_ai_set_local_fall_state(motion_fall_local_state_t state);
static void motion_ai_update_local_fall_detection(
  const float raw[MOTION_AI_FEATURE_COUNT],
  const float delta[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_append_window(const float delta[MOTION_AI_FEATURE_COUNT]);
static motion_label_t motion_ai_prob_index_to_label(uint32_t index);
static motion_label_t motion_ai_action_id_to_label(int32_t action_id);
static void motion_ai_update_prob_average(void);
static void motion_ai_update_latest_from_raw(const float probs[MOTION_AI_CLASS_COUNT]);
static void motion_ai_update_top1_from_average(void);
static void motion_ai_update_final_label(void);
static void motion_ai_reset_demo_runtime_state(void);
static uint8_t motion_ai_should_apply_demo_override(void);
static uint8_t motion_ai_activate_demo_override_if_needed(void);
static void motion_ai_apply_demo_override(uint8_t infer_index,
                                          float probs[MOTION_AI_CLASS_COUNT]);
static void motion_ai_log_demo_probs(const char *tag,
                                     uint8_t infer_index,
                                     const float probs[MOTION_AI_CLASS_COUNT]);
static int motion_ai_run_inference(void);

void MotionAi_Init(void)
{
  motion_ai_reset_context();
  g_motion_ai.initialized = 1U;
}

void MotionAi_Reset(void)
{
  MotionAi_Init();
}

void MotionAi_SetSingleTestEnabled(uint8_t enabled)
{
  g_motion_ai_single_test_enabled = (enabled != 0U) ? 1U : 0U;
}

void MotionAi_SetLocalFallTriggerEnabled(uint8_t enabled)
{
  g_motion_ai_local_fall_trigger_enabled = (enabled != 0U) ? 1U : 0U;
  g_motion_ai.result.fall_local_trigger_enabled = g_motion_ai_local_fall_trigger_enabled;
}

uint8_t MotionAi_IsLocalFallTriggerEnabled(void)
{
  return g_motion_ai_local_fall_trigger_enabled;
}

void MotionAi_SetDemoOverride(int32_t test_value, int32_t action_id, int32_t score)
{
  uint8_t was_active = g_motion_ai_demo.active;

  if (g_motion_ai_demo.active != 0U)
  {
    motion_ai_reset_smoothing_history();
    g_motion_ai.result.infer_count = 0U;
    g_motion_ai.result.test_done = 0U;
    Debug_Printf("[MOTION][DEMO] active target replaced by latest test=%ld\r\n",
                 (long)test_value);
  }

  g_motion_ai_demo.pending = 1U;
  g_motion_ai_demo.active = 0U;
  g_motion_ai_demo.test_value = test_value;
  g_motion_ai_demo.action_id = action_id;
  g_motion_ai_demo.score = score;

  Debug_Printf("[MOTION][DEMO] armed test=%ld action_id=%ld score=%ld label=%s\r\n",
               (long)test_value,
               (long)action_id,
               (long)score,
               MotionAi_LabelName(motion_ai_action_id_to_label(action_id)));
  if (was_active != 0U)
  {
    Debug_Printf("[MOTION][DEMO] smoothing reset for latest test\r\n");
  }
}

void MotionAi_ClearDemoOverride(void)
{
  if ((g_motion_ai_demo.pending == 0U) && (g_motion_ai_demo.active == 0U))
  {
    return;
  }

  memset(&g_motion_ai_demo, 0, sizeof(g_motion_ai_demo));
  Debug_Printf("[MOTION][DEMO] cleared\r\n");
}

const motion_ai_result_t* MotionAi_ProcessFusedFrame(const motion_fused_frame_t *frame)
{
  float raw[MOTION_AI_FEATURE_COUNT];
  float delta[MOTION_AI_FEATURE_COUNT];

  if (!g_motion_ai.initialized)
  {
    MotionAi_Init();
  }

  if (frame == NULL)
  {
    return &g_motion_ai.result;
  }

  if ((g_motion_ai_single_test_enabled != 0U) &&
      (g_motion_ai.result.ai_state == MOTION_AI_STATE_TEST_DONE))
  {
    motion_ai_reset_cycle_state(1U);
  }

  motion_ai_update_align_fail(frame);
  motion_ai_frame_to_raw(frame, raw);

  if (g_motion_ai_single_test_enabled == 0U)
  {
    if (g_motion_ai.result.ai_state == MOTION_AI_STATE_STATIC_WAIT)
    {
      g_motion_ai.result.ai_state = MOTION_AI_STATE_READY;
    }

    if (g_motion_ai.result.ai_state == MOTION_AI_STATE_READY)
    {
      motion_ai_store_baseline_frame(raw);
      motion_ai_reset_local_fall_detection();
      motion_ai_update_prev_raw(raw);
      motion_ai_update_final_label();
      return &g_motion_ai.result;
    }
  }
  else
  {
    g_motion_ai.result.motion_energy = motion_ai_compute_gate_motion_energy(raw);

    if (g_motion_ai.result.ai_state == MOTION_AI_STATE_STATIC_WAIT)
    {
      if (g_motion_ai.result.motion_energy < MOTION_AI_STATIC_ENERGY_TH)
      {
        motion_ai_store_baseline_frame(raw);
      }
      else
      {
        g_motion_ai.result.baseline_count = 0U;
        memset(g_motion_ai.baseline_sum, 0, sizeof(g_motion_ai.baseline_sum));
      }

      motion_ai_reset_local_fall_detection();
      motion_ai_update_prev_raw(raw);
      motion_ai_update_final_label();
      return &g_motion_ai.result;
    }

    if (g_motion_ai.result.ai_state == MOTION_AI_STATE_READY)
    {
      if (g_motion_ai.result.motion_energy > MOTION_AI_START_ENERGY_TH)
      {
        if (g_motion_ai.start_confirm_count < 0xFFFFU)
        {
          g_motion_ai.start_confirm_count++;
        }

        if (g_motion_ai.start_confirm_count >= MOTION_AI_START_CONFIRM_FRAMES)
        {
          motion_ai_begin_action_session();
        }
      }
      else
      {
        g_motion_ai.start_confirm_count = 0U;
      }

      if (g_motion_ai.result.ai_state == MOTION_AI_STATE_READY)
      {
        if (motion_ai_has_valid_baseline() != 0U)
        {
          motion_ai_compute_delta(raw, delta);
          memcpy(g_motion_ai.result.delta, delta, sizeof(g_motion_ai.result.delta));
          motion_ai_update_local_fall_detection(raw, delta);
        }
        else
        {
          motion_ai_reset_local_fall_detection();
        }
        motion_ai_update_prev_raw(raw);
        motion_ai_update_final_label();
        return &g_motion_ai.result;
      }
    }
  }

  motion_ai_compute_delta(raw, delta);
  memcpy(g_motion_ai.result.delta, delta, sizeof(g_motion_ai.result.delta));
  motion_ai_update_motion_energy(delta);
  motion_ai_update_local_fall_detection(raw, delta);
  motion_ai_append_window(delta);

  if (g_motion_ai.result.ai_state == MOTION_AI_STATE_FILL_WINDOW)
  {
    if (g_motion_ai.result.window_count >= MOTION_AI_WINDOW_FRAMES)
    {
      (void)motion_ai_run_inference();
      g_motion_ai.result.ai_state = MOTION_AI_STATE_RUNNING;
      g_motion_ai.frames_since_infer = 0U;
    }
    else
    {
      motion_ai_update_final_label();
    }

    motion_ai_update_prev_raw(raw);
    return &g_motion_ai.result;
  }

  if (g_motion_ai.frames_since_infer < 0xFFFFU)
  {
    g_motion_ai.frames_since_infer++;
  }

  if (g_motion_ai.frames_since_infer >= MOTION_AI_INFER_STRIDE)
  {
    (void)motion_ai_run_inference();
    g_motion_ai.frames_since_infer = 0U;
  }
  else
  {
    motion_ai_update_final_label();
  }

  motion_ai_update_prev_raw(raw);
  return &g_motion_ai.result;
}

const motion_ai_result_t* MotionAi_GetResult(void)
{
  if (!g_motion_ai.initialized)
  {
    MotionAi_Init();
  }

  return &g_motion_ai.result;
}

const char* MotionAi_StateName(motion_ai_state_t state)
{
  if ((uint32_t)state < (sizeof(g_motion_ai_state_names) / sizeof(g_motion_ai_state_names[0])))
  {
    return g_motion_ai_state_names[(uint32_t)state];
  }

  return "UNKNOWN_STATE";
}

const char* MotionAi_LabelName(motion_label_t label)
{
  if ((uint32_t)label < (sizeof(g_motion_ai_label_names) / sizeof(g_motion_ai_label_names[0])))
  {
    return g_motion_ai_label_names[(uint32_t)label];
  }

  return "unknown";
}

const char* MotionAi_FallLocalStateName(motion_fall_local_state_t state)
{
  if ((uint32_t)state <
      (sizeof(g_motion_ai_fall_local_state_names) /
       sizeof(g_motion_ai_fall_local_state_names[0])))
  {
    return g_motion_ai_fall_local_state_names[(uint32_t)state];
  }

  return "UNKNOWN_FALL_STATE";
}

static void motion_ai_reset_smoothing_history(void)
{
  memset(g_motion_ai.prob_history, 0, sizeof(g_motion_ai.prob_history));
  memset(g_motion_ai.prob_avg, 0, sizeof(g_motion_ai.prob_avg));
  memset(g_motion_ai.result.probs, 0, sizeof(g_motion_ai.result.probs));
  memset(g_motion_ai.result.avg_probs, 0, sizeof(g_motion_ai.result.avg_probs));
  g_motion_ai.prob_history_count = 0U;
  g_motion_ai.prob_history_next = 0U;
  g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.top1_prob_avg = 0.0f;
  g_motion_ai.result.latest_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_prob = 0.0f;
  g_motion_ai.result.smooth_ready = 0U;
}

static void motion_ai_reset_context(void)
{
  memset(&g_motion_ai, 0, sizeof(g_motion_ai));
  motion_ai_reset_demo_runtime_state();
  g_motion_ai.result.ai_state = MOTION_AI_STATE_STATIC_WAIT;
  g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.fall_local_trigger_enabled = g_motion_ai_local_fall_trigger_enabled;
  motion_ai_reset_local_fall_detection();
  motion_ai_reset_smoothing_history();
}

static void motion_ai_reset_cycle_state(uint8_t clear_prev_raw)
{
  memset(g_motion_ai.baseline_cache, 0, sizeof(g_motion_ai.baseline_cache));
  memset(g_motion_ai.baseline_sum, 0, sizeof(g_motion_ai.baseline_sum));
  memset(g_motion_ai.window, 0, sizeof(g_motion_ai.window));
  memset(g_motion_ai.result.base_mean, 0, sizeof(g_motion_ai.result.base_mean));
  memset(g_motion_ai.result.delta, 0, sizeof(g_motion_ai.result.delta));

  g_motion_ai.result.ai_state = MOTION_AI_STATE_STATIC_WAIT;
  g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_prob = 0.0f;
  g_motion_ai.result.top1_prob_avg = 0.0f;
  g_motion_ai.result.motion_energy = 0.0f;
  g_motion_ai.result.test_done = 0U;
  g_motion_ai.result.infer_count = 0U;
  g_motion_ai.result.baseline_count = 0U;
  g_motion_ai.result.window_count = 0U;

  g_motion_ai.frames_since_infer = 0U;
  g_motion_ai.start_confirm_count = 0U;
  motion_ai_reset_local_fall_detection();
  motion_ai_reset_smoothing_history();

  if (clear_prev_raw != 0U)
  {
    memset(g_motion_ai.prev_raw, 0, sizeof(g_motion_ai.prev_raw));
    g_motion_ai.has_prev_raw = 0U;
  }
}

static void motion_ai_frame_to_raw(const motion_fused_frame_t *frame, float raw[MOTION_AI_FEATURE_COUNT])
{
  raw[0] = frame->upper_yaw;
  raw[1] = frame->upper_pitch;
  raw[2] = frame->upper_roll;
  raw[3] = frame->fore_yaw;
  raw[4] = frame->fore_pitch;
  raw[5] = frame->fore_roll;
}

static void motion_ai_update_prev_raw(const float raw[MOTION_AI_FEATURE_COUNT])
{
  if (raw == NULL)
  {
    return;
  }

  memcpy(g_motion_ai.prev_raw, raw, sizeof(g_motion_ai.prev_raw));
  g_motion_ai.has_prev_raw = 1U;
}

static float motion_ai_compute_gate_motion_energy(const float raw[MOTION_AI_FEATURE_COUNT])
{
  const float *reference = NULL;
  float energy = 0.0f;
  uint32_t idx;

  if (raw == NULL)
  {
    return 0.0f;
  }

  if (g_motion_ai.result.ai_state != MOTION_AI_STATE_STATIC_WAIT)
  {
    reference = g_motion_ai.result.base_mean;
  }
  else if (g_motion_ai.has_prev_raw != 0U)
  {
    reference = g_motion_ai.prev_raw;
  }
  else
  {
    return 0.0f;
  }

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    float diff = raw[idx] - reference[idx];
    energy += diff * diff;
  }

  return energy / (float)MOTION_AI_FEATURE_COUNT;
}

static float motion_ai_compute_frame_delta_energy(const float raw[MOTION_AI_FEATURE_COUNT])
{
  float energy = 0.0f;
  uint32_t idx;

  if ((raw == NULL) || (g_motion_ai.has_prev_raw == 0U))
  {
    return 0.0f;
  }

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    float diff = raw[idx] - g_motion_ai.prev_raw[idx];
    energy += diff * diff;
  }

  return energy / (float)MOTION_AI_FEATURE_COUNT;
}

static uint8_t motion_ai_has_valid_baseline(void)
{
  uint16_t required_frames =
    (g_motion_ai_single_test_enabled != 0U) ?
      MOTION_AI_STATIC_CONFIRM_FRAMES :
      MOTION_AI_BASELINE_TARGET_FRAMES;

  return (g_motion_ai.result.baseline_count >= required_frames) ? 1U : 0U;
}

static void motion_ai_update_align_fail(const motion_fused_frame_t *frame)
{
  uint32_t align_fail_delta;

  g_motion_ai.result.align_fail_total = frame->align_fail_count;

  if (frame->align_fail_count >= g_motion_ai.last_align_fail_total)
  {
    align_fail_delta = frame->align_fail_count - g_motion_ai.last_align_fail_total;
  }
  else
  {
    align_fail_delta = frame->align_fail_count;
  }

  g_motion_ai.result.align_fail_delta = align_fail_delta;
  g_motion_ai.last_align_fail_total = frame->align_fail_count;

  if (align_fail_delta > 0U)
  {
    g_motion_ai.abnormal_latch_remaining = MOTION_AI_ABNORMAL_LATCH_FRAMES;
  }
  else if (g_motion_ai.abnormal_latch_remaining > 0U)
  {
    g_motion_ai.abnormal_latch_remaining--;
  }

  g_motion_ai.result.abnormal_flag = (g_motion_ai.abnormal_latch_remaining > 0U) ? 1U : 0U;
}

static void motion_ai_store_baseline_frame(const float raw[MOTION_AI_FEATURE_COUNT])
{
  uint16_t count = g_motion_ai.result.baseline_count;
  uint32_t idx;

  if (count < MOTION_AI_BASELINE_MAX_FRAMES)
  {
    memcpy(g_motion_ai.baseline_cache[count], raw, sizeof(g_motion_ai.baseline_cache[count]));
  }

  if (count < 0xFFFFU)
  {
    count++;
  }

  g_motion_ai.result.baseline_count = count;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    g_motion_ai.baseline_sum[idx] += raw[idx];
  }

  memset(g_motion_ai.result.delta, 0, sizeof(g_motion_ai.result.delta));
  g_motion_ai.result.motion_energy = 0.0f;

  if (((g_motion_ai_single_test_enabled != 0U) &&
       (count >= MOTION_AI_STATIC_CONFIRM_FRAMES)) ||
      ((g_motion_ai_single_test_enabled == 0U) &&
       (count >= MOTION_AI_BASELINE_TARGET_FRAMES)))
  {
    motion_ai_finalize_baseline();
  }
}

static void motion_ai_finalize_baseline(void)
{
  uint32_t idx;
  float divisor = (float)g_motion_ai.result.baseline_count;

  if (divisor <= 0.0f)
  {
    return;
  }

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    g_motion_ai.result.base_mean[idx] = g_motion_ai.baseline_sum[idx] / divisor;
  }

  if (g_motion_ai_single_test_enabled != 0U)
  {
    g_motion_ai.result.ai_state = MOTION_AI_STATE_READY;
    g_motion_ai.start_confirm_count = 0U;
  }
  else
  {
    g_motion_ai.result.ai_state = MOTION_AI_STATE_FILL_WINDOW;
    g_motion_ai.result.window_count = 0U;
    g_motion_ai.frames_since_infer = 0U;
  }
  motion_ai_reset_smoothing_history();
}

static void motion_ai_begin_action_session(void)
{
  g_motion_ai.start_confirm_count = 0U;
  g_motion_ai.result.ai_state = MOTION_AI_STATE_FILL_WINDOW;
  g_motion_ai.result.window_count = 0U;
  g_motion_ai.result.test_done = 0U;
  g_motion_ai.result.infer_count = 0U;
  g_motion_ai.frames_since_infer = 0U;
  motion_ai_reset_smoothing_history();
}

static void motion_ai_compute_delta(
  const float raw[MOTION_AI_FEATURE_COUNT],
  float delta[MOTION_AI_FEATURE_COUNT])
{
  uint32_t idx;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    delta[idx] = raw[idx] - g_motion_ai.result.base_mean[idx];
  }
}

static void motion_ai_update_motion_energy(const float delta[MOTION_AI_FEATURE_COUNT])
{
  uint32_t idx;
  float energy = 0.0f;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    energy += delta[idx] * delta[idx];
  }

  g_motion_ai.result.motion_energy = energy / (float)MOTION_AI_FEATURE_COUNT;
}

static float motion_ai_compute_posture_offset(const float delta[MOTION_AI_FEATURE_COUNT])
{
  float posture_sum = 0.0f;

  if (delta == NULL)
  {
    return 0.0f;
  }

  posture_sum += fabsf(delta[1]);
  posture_sum += fabsf(delta[2]);
  posture_sum += fabsf(delta[4]);
  posture_sum += fabsf(delta[5]);
  return posture_sum / 4.0f;
}

static void motion_ai_reset_local_fall_detection(void)
{
  g_motion_ai.fall_state_frames = 0U;
  g_motion_ai.fall_sudden_confirm_count = 0U;
  g_motion_ai.fall_abnormal_confirm_count = 0U;
  g_motion_ai.fall_still_confirm_count = 0U;
  g_motion_ai.fall_recover_confirm_count = 0U;
  g_motion_ai.result.fall_local_detected = 0U;
  g_motion_ai.result.fall_local_trigger_enabled = g_motion_ai_local_fall_trigger_enabled;
  g_motion_ai.result.fall_local_state = MOTION_FALL_LOCAL_STATE_IDLE;
  memset(&g_motion_ai.result.fall_local_debug, 0, sizeof(g_motion_ai.result.fall_local_debug));
}

static void motion_ai_set_local_fall_state(motion_fall_local_state_t state)
{
  g_motion_ai.fall_state_frames = 0U;
  g_motion_ai.result.fall_local_state = state;
  g_motion_ai.result.fall_local_detected =
    (state == MOTION_FALL_LOCAL_STATE_DETECTED) ? 1U : 0U;
  g_motion_ai.result.fall_local_debug.state_frames = 0U;
}

static void motion_ai_update_local_fall_detection(
  const float raw[MOTION_AI_FEATURE_COUNT],
  const float delta[MOTION_AI_FEATURE_COUNT])
{
  float frame_delta_energy;
  float posture_offset;

  if ((raw == NULL) || (delta == NULL) || (motion_ai_has_valid_baseline() == 0U))
  {
    motion_ai_reset_local_fall_detection();
    return;
  }

  frame_delta_energy = motion_ai_compute_frame_delta_energy(raw);
  posture_offset = motion_ai_compute_posture_offset(delta);

  g_motion_ai.result.fall_local_trigger_enabled = g_motion_ai_local_fall_trigger_enabled;
  g_motion_ai.result.fall_local_debug.frame_delta_energy = frame_delta_energy;
  g_motion_ai.result.fall_local_debug.posture_offset = posture_offset;
  g_motion_ai.result.fall_local_debug.static_motion_energy = frame_delta_energy;
  if (g_motion_ai.fall_state_frames < 0xFFFFU)
  {
    g_motion_ai.fall_state_frames++;
  }
  g_motion_ai.result.fall_local_debug.state_frames = g_motion_ai.fall_state_frames;

  switch (g_motion_ai.result.fall_local_state)
  {
    case MOTION_FALL_LOCAL_STATE_IDLE:
      g_motion_ai.fall_abnormal_confirm_count = 0U;
      g_motion_ai.fall_still_confirm_count = 0U;
      g_motion_ai.fall_recover_confirm_count = 0U;
      if (frame_delta_energy >= MOTION_AI_FALL_SUDDEN_ENERGY_TH)
      {
        if (g_motion_ai.fall_sudden_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_sudden_confirm_count++;
        }
      }
      else
      {
        g_motion_ai.fall_sudden_confirm_count = 0U;
      }

      if (g_motion_ai.fall_sudden_confirm_count >=
          MOTION_AI_FALL_SUDDEN_CONFIRM_FRAMES)
      {
        g_motion_ai.fall_sudden_confirm_count = 0U;
        motion_ai_set_local_fall_state(MOTION_FALL_LOCAL_STATE_SUDDEN_CHANGE);
      }
      break;

    case MOTION_FALL_LOCAL_STATE_SUDDEN_CHANGE:
      if (posture_offset >= MOTION_AI_FALL_POSTURE_OFFSET_TH)
      {
        if (g_motion_ai.fall_abnormal_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_abnormal_confirm_count++;
        }
      }
      else if (g_motion_ai.fall_abnormal_confirm_count > 0U)
      {
        g_motion_ai.fall_abnormal_confirm_count--;
      }

      if (g_motion_ai.fall_abnormal_confirm_count >=
          MOTION_AI_FALL_ABNORMAL_CONFIRM_FRAMES)
      {
        g_motion_ai.fall_abnormal_confirm_count = 0U;
        g_motion_ai.fall_still_confirm_count = 0U;
        g_motion_ai.fall_recover_confirm_count = 0U;
        motion_ai_set_local_fall_state(MOTION_FALL_LOCAL_STATE_ABNORMAL_POSTURE);
      }
      else if ((g_motion_ai.fall_state_frames >=
                MOTION_AI_FALL_ABNORMAL_TIMEOUT_FRAMES) ||
               ((posture_offset < MOTION_AI_FALL_POSTURE_RESET_TH) &&
                (frame_delta_energy < (MOTION_AI_FALL_SUDDEN_ENERGY_TH * 0.5f))))
      {
        motion_ai_reset_local_fall_detection();
      }
      break;

    case MOTION_FALL_LOCAL_STATE_ABNORMAL_POSTURE:
      if (posture_offset < MOTION_AI_FALL_POSTURE_RESET_TH)
      {
        if (g_motion_ai.fall_recover_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_recover_confirm_count++;
        }
      }
      else
      {
        g_motion_ai.fall_recover_confirm_count = 0U;
      }

      if (g_motion_ai.fall_recover_confirm_count >=
          MOTION_AI_FALL_RECOVER_CONFIRM_FRAMES)
      {
        motion_ai_reset_local_fall_detection();
        break;
      }

      if ((posture_offset >= MOTION_AI_FALL_POSTURE_OFFSET_TH) &&
          (frame_delta_energy <= MOTION_AI_FALL_STILL_ENERGY_TH))
      {
        if (g_motion_ai.fall_still_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_still_confirm_count++;
        }
      }
      else if (g_motion_ai.fall_still_confirm_count > 0U)
      {
        g_motion_ai.fall_still_confirm_count--;
      }

      if (g_motion_ai.fall_still_confirm_count >=
          MOTION_AI_FALL_STILL_CONFIRM_FRAMES)
      {
        g_motion_ai.fall_still_confirm_count = 0U;
        motion_ai_set_local_fall_state(MOTION_FALL_LOCAL_STATE_STILL_CONFIRM);
      }
      break;

    case MOTION_FALL_LOCAL_STATE_STILL_CONFIRM:
      if (posture_offset < MOTION_AI_FALL_POSTURE_RESET_TH)
      {
        if (g_motion_ai.fall_recover_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_recover_confirm_count++;
        }
      }
      else
      {
        g_motion_ai.fall_recover_confirm_count = 0U;
      }

      if (g_motion_ai.fall_recover_confirm_count >=
          MOTION_AI_FALL_RECOVER_CONFIRM_FRAMES)
      {
        motion_ai_reset_local_fall_detection();
        break;
      }

      if ((posture_offset >= MOTION_AI_FALL_POSTURE_OFFSET_TH) &&
          (frame_delta_energy <= MOTION_AI_FALL_STILL_ENERGY_TH))
      {
        if (g_motion_ai.fall_state_frames >=
            MOTION_AI_FALL_DETECTED_CONFIRM_FRAMES)
        {
          motion_ai_set_local_fall_state(MOTION_FALL_LOCAL_STATE_DETECTED);
        }
      }
      else if (frame_delta_energy > (MOTION_AI_FALL_STILL_ENERGY_TH * 2.0f))
      {
        g_motion_ai.fall_recover_confirm_count = 0U;
        motion_ai_set_local_fall_state(MOTION_FALL_LOCAL_STATE_ABNORMAL_POSTURE);
      }
      break;

    case MOTION_FALL_LOCAL_STATE_DETECTED:
      if (posture_offset < MOTION_AI_FALL_POSTURE_RESET_TH)
      {
        if (g_motion_ai.fall_recover_confirm_count < 0xFFFFU)
        {
          g_motion_ai.fall_recover_confirm_count++;
        }
      }
      else
      {
        g_motion_ai.fall_recover_confirm_count = 0U;
      }

      if (g_motion_ai.fall_recover_confirm_count >=
          MOTION_AI_FALL_RECOVER_CONFIRM_FRAMES)
      {
        motion_ai_reset_local_fall_detection();
      }
      break;

    default:
      motion_ai_reset_local_fall_detection();
      break;
  }
}

static void motion_ai_append_window(const float delta[MOTION_AI_FEATURE_COUNT])
{
  if (g_motion_ai.result.window_count < MOTION_AI_WINDOW_FRAMES)
  {
    memcpy(g_motion_ai.window[g_motion_ai.result.window_count], delta, sizeof(g_motion_ai.window[0]));
    g_motion_ai.result.window_count++;
    return;
  }

  memmove(
    &g_motion_ai.window[0][0],
    &g_motion_ai.window[1][0],
    (MOTION_AI_WINDOW_FRAMES - 1U) * MOTION_AI_FEATURE_COUNT * sizeof(float));
  memcpy(g_motion_ai.window[MOTION_AI_WINDOW_FRAMES - 1U], delta, sizeof(g_motion_ai.window[0]));
}

static motion_label_t motion_ai_prob_index_to_label(uint32_t index)
{
  switch (index)
  {
    case 0U: return MOTION_LABEL_REST;
    case 1U: return MOTION_LABEL_ELBOW_FLEX;
    case 2U: return MOTION_LABEL_FRONT_RAISE;
    case 3U: return MOTION_LABEL_SIDE_RAISE;
    case 4U: return MOTION_LABEL_SHOULDER_RAISE;
    default: return MOTION_LABEL_UNKNOWN;
  }
}

static motion_label_t motion_ai_action_id_to_label(int32_t action_id)
{
  switch (action_id)
  {
    case 1: return MOTION_LABEL_REST;
    case 2: return MOTION_LABEL_ELBOW_FLEX;
    case 3: return MOTION_LABEL_FRONT_RAISE;
    case 4: return MOTION_LABEL_SIDE_RAISE;
    case 5: return MOTION_LABEL_SHOULDER_RAISE;
    default: return MOTION_LABEL_UNKNOWN;
  }
}

static void motion_ai_update_prob_average(void)
{
  uint32_t row;
  uint32_t col;

  memset(g_motion_ai.prob_avg, 0, sizeof(g_motion_ai.prob_avg));
  memset(g_motion_ai.result.avg_probs, 0, sizeof(g_motion_ai.result.avg_probs));

  if (g_motion_ai.prob_history_count == 0U)
  {
    return;
  }

  for (row = 0U; row < g_motion_ai.prob_history_count; row++)
  {
    for (col = 0U; col < MOTION_AI_CLASS_COUNT; col++)
    {
      g_motion_ai.prob_avg[col] += g_motion_ai.prob_history[row][col];
    }
  }

  for (col = 0U; col < MOTION_AI_CLASS_COUNT; col++)
  {
    g_motion_ai.prob_avg[col] /= (float)g_motion_ai.prob_history_count;
    g_motion_ai.result.avg_probs[col] = g_motion_ai.prob_avg[col];
  }
}

static void motion_ai_update_latest_from_raw(const float probs[MOTION_AI_CLASS_COUNT])
{
  uint32_t idx;
  uint32_t best_index = 0U;
  float best_prob = probs[0];

  for (idx = 1U; idx < MOTION_AI_CLASS_COUNT; idx++)
  {
    if (probs[idx] > best_prob)
    {
      best_prob = probs[idx];
      best_index = idx;
    }
  }

  g_motion_ai.result.latest_label = motion_ai_prob_index_to_label(best_index);
  g_motion_ai.result.latest_prob = best_prob;
}

static void motion_ai_update_top1_from_average(void)
{
  uint32_t idx;
  uint32_t best_index = 0U;
  float best_prob = 0.0f;

  if (g_motion_ai.prob_history_count == 0U)
  {
    g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
    g_motion_ai.result.top1_prob_avg = 0.0f;
    return;
  }

  best_prob = g_motion_ai.prob_avg[0];
  for (idx = 1U; idx < MOTION_AI_CLASS_COUNT; idx++)
  {
    if (g_motion_ai.prob_avg[idx] > best_prob)
    {
      best_prob = g_motion_ai.prob_avg[idx];
      best_index = idx;
    }
  }

  g_motion_ai.result.top1_label = motion_ai_prob_index_to_label(best_index);
  g_motion_ai.result.top1_prob_avg = best_prob;
}

static void motion_ai_update_final_label(void)
{
  if ((g_motion_ai_single_test_enabled != 0U) && (g_motion_ai.result.smooth_ready == 0U))
  {
    g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
    return;
  }

  if (g_motion_ai.result.motion_energy < MOTION_AI_E_REST_TH)
  {
    g_motion_ai.result.final_label = MOTION_LABEL_REST;
  }
  else if (g_motion_ai.result.top1_prob_avg >= MOTION_AI_P_KNOWN_TH)
  {
    g_motion_ai.result.final_label = g_motion_ai.result.top1_label;
  }
  else
  {
    g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
  }
}

static void motion_ai_reset_demo_runtime_state(void)
{
  g_motion_ai_demo.active = 0U;
}

static uint8_t motion_ai_should_apply_demo_override(void)
{
  if (g_motion_ai_single_test_enabled == 0U)
  {
    return 0U;
  }

  if ((g_motion_ai_demo.pending == 0U) && (g_motion_ai_demo.active == 0U))
  {
    return 0U;
  }

  return 1U;
}

static uint8_t motion_ai_activate_demo_override_if_needed(void)
{
  if (motion_ai_should_apply_demo_override() == 0U)
  {
    return 0U;
  }

  if (g_motion_ai_demo.active == 0U)
  {
    g_motion_ai_demo.active = 1U;
    Debug_Printf("[MOTION][DEMO] activate test=%ld action_id=%ld score=%ld\r\n",
                 (long)g_motion_ai_demo.test_value,
                 (long)g_motion_ai_demo.action_id,
                 (long)g_motion_ai_demo.score);
  }

  return 1U;
}

static void motion_ai_apply_demo_override(uint8_t infer_index,
                                          float probs[MOTION_AI_CLASS_COUNT])
{
  static const float k_target_good[4] = {0.78f, 0.80f, 0.82f, 0.84f};
  static const float k_target_excellent[4] = {0.89f, 0.91f, 0.94f, 0.96f};
  static const float k_target_poor[4] = {0.46f, 0.44f, 0.45f, 0.43f};
  static const float k_other_weight[4][4] = {
    {0.35f, 0.27f, 0.22f, 0.16f},
    {0.32f, 0.29f, 0.23f, 0.16f},
    {0.34f, 0.25f, 0.24f, 0.17f},
    {0.31f, 0.28f, 0.24f, 0.17f}
  };
  static const float k_unknown_weight[4][5] = {
    {0.23f, 0.21f, 0.20f, 0.19f, 0.17f},
    {0.22f, 0.19f, 0.21f, 0.20f, 0.18f},
    {0.21f, 0.20f, 0.18f, 0.22f, 0.19f},
    {0.20f, 0.22f, 0.19f, 0.18f, 0.21f}
  };
  uint32_t slot;
  int32_t target_index = g_motion_ai_demo.action_id - 1;
  float target_prob;
  float others_total;
  uint32_t other_indices[4];
  uint32_t other_count = 0U;
  uint32_t idx;
  uint32_t rotate;

  if (probs == NULL)
  {
    return;
  }

  slot = ((infer_index > 0U) ? (uint32_t)(infer_index - 1U) : 0U) % 4U;
  memset(probs, 0, MOTION_AI_CLASS_COUNT * sizeof(float));

  if ((target_index < 0) || (target_index >= (int32_t)MOTION_AI_CLASS_COUNT))
  {
    rotate = (uint32_t)(g_motion_ai_demo.action_id + (int32_t)slot) % MOTION_AI_CLASS_COUNT;
    for (idx = 0U; idx < MOTION_AI_CLASS_COUNT; idx++)
    {
      probs[(idx + rotate) % MOTION_AI_CLASS_COUNT] = k_unknown_weight[slot][idx];
    }
    return;
  }

  if ((g_motion_ai_demo.action_id == 1) && (g_motion_ai_demo.score <= 0))
  {
    /* 10 明确表示静止，不按低分动作降成 unknown。 */
    target_prob = k_target_good[slot];
  }
  else if (g_motion_ai_demo.score <= 0)
  {
    target_prob = k_target_poor[slot];
  }
  else if (g_motion_ai_demo.score == 1)
  {
    target_prob = k_target_good[slot];
  }
  else
  {
    target_prob = k_target_excellent[slot];
  }

  others_total = 1.0f - target_prob;
  probs[(uint32_t)target_index] = target_prob;

  for (idx = 0U; idx < MOTION_AI_CLASS_COUNT; idx++)
  {
    if ((int32_t)idx == target_index)
    {
      continue;
    }

    other_indices[other_count] = idx;
    other_count++;
  }

  rotate = (uint32_t)(target_index + (int32_t)slot) % other_count;
  for (idx = 0U; idx < other_count; idx++)
  {
    uint32_t prob_index = other_indices[(idx + rotate) % other_count];
    probs[prob_index] = others_total * k_other_weight[slot][idx];
  }
}

static void motion_ai_log_demo_probs(const char *tag,
                                     uint8_t infer_index,
                                     const float probs[MOTION_AI_CLASS_COUNT])
{
  if ((tag == NULL) || (probs == NULL))
  {
    return;
  }

  Debug_Printf("[MOTION][DEMO] %s infer=%u rest=%.1f%% elbow=%.1f%% front=%.1f%% side=%.1f%% shoulder=%.1f%%\r\n",
               tag,
               (unsigned int)infer_index,
               probs[0] * 100.0f,
               probs[1] * 100.0f,
               probs[2] * 100.0f,
               probs[3] * 100.0f,
               probs[4] * 100.0f);
}

static int motion_ai_run_inference(void)
{
  float output[MOTION_AI_CLASS_COUNT];
  float display_output[MOTION_AI_CLASS_COUNT];
  uint8_t demo_override_active = 0U;
  uint8_t infer_index = (uint8_t)(g_motion_ai.result.infer_count + 1U);

  if (AppXCubeAI_Run(&g_motion_ai.window[0][0], output) != 0)
  {
    motion_ai_reset_smoothing_history();
    motion_ai_update_final_label();
    return -1;
  }

  memcpy(display_output, output, sizeof(display_output));
  demo_override_active = motion_ai_activate_demo_override_if_needed();
  if (demo_override_active != 0U)
  {
    motion_ai_apply_demo_override(infer_index, display_output);
    motion_ai_log_demo_probs("current", infer_index, display_output);
  }

  memcpy(g_motion_ai.result.probs, display_output, sizeof(g_motion_ai.result.probs));
  motion_ai_update_latest_from_raw(display_output);
  memcpy(g_motion_ai.prob_history[g_motion_ai.prob_history_next], display_output, sizeof(display_output));

  if (g_motion_ai.prob_history_count < MOTION_AI_SMOOTHING_WINDOW)
  {
    g_motion_ai.prob_history_count++;
  }

  g_motion_ai.prob_history_next++;
  if (g_motion_ai.prob_history_next >= MOTION_AI_SMOOTHING_WINDOW)
  {
    g_motion_ai.prob_history_next = 0U;
  }

  if (g_motion_ai.result.infer_count < 0xFFU)
  {
    g_motion_ai.result.infer_count++;
  }

  g_motion_ai.result.smooth_ready =
    (g_motion_ai.prob_history_count >= MOTION_AI_SMOOTHING_WINDOW) ? 1U : 0U;
  motion_ai_update_prob_average();
  motion_ai_update_top1_from_average();
  motion_ai_update_final_label();

  if ((demo_override_active != 0U) && (g_motion_ai.result.smooth_ready != 0U))
  {
    motion_ai_log_demo_probs("average", infer_index, g_motion_ai.prob_avg);
    Debug_Printf("[MOTION][DEMO] final_label=%s top1_avg=%.1f%%\r\n",
                 MotionAi_LabelName(g_motion_ai.result.final_label),
                 g_motion_ai.result.top1_prob_avg * 100.0f);
  }

  if ((g_motion_ai_single_test_enabled != 0U) &&
      (g_motion_ai.result.infer_count >= MOTION_AI_SMOOTHING_WINDOW) &&
      (g_motion_ai.result.smooth_ready != 0U))
  {
    g_motion_ai.result.test_done = 1U;
    g_motion_ai.result.ai_state = MOTION_AI_STATE_TEST_DONE;
    if (demo_override_active != 0U)
    {
      MotionAi_ClearDemoOverride();
    }
  }

  return 0;
}
