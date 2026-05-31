#include "motion_task.h"

#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "debug_uart7.h"
#include "motion_ai.h"
#include "motion_app_events.h"
#include "motion_input.h"
#include "motion_mode.h"
#include "motion_sensor_pipeline.h"
#include "motion_window_test.h"
#include "onenet.h"
#include "rule_action_recognizer.h"
#include "uart7_role.h"

#define FALL_WARNING_INTERVAL_MS 500U

static uint8_t task1_consume_ai_restart_request(void);
static uint8_t task1_try_take_fused_frame(motion_fused_frame_t *frame);
static uint8_t task1_try_run_model_window_test(void);
static uint8_t task1_mode_uses_single_test(motion_output_mode_t mode);
static void task1_output_state_reset(motion_output_mode_t mode, uint8_t fresh_session);
static void task1_reset_recognition_state(void);
static int32_t task1_encode_action_kind_value(int32_t test_value);
static void task1_process_fused_frame(const motion_fused_frame_t *frame);
static void task1_handle_local_fall_result(const motion_ai_result_t *result);
static void task1_output_capture_csv(const motion_fused_frame_t *frame);
static void task1_output_recognition_csv(const motion_fused_frame_t *frame, const motion_ai_result_t *result);
static void task1_output_single_once_event(const motion_fused_frame_t *frame, const motion_ai_result_t *result);
static void task1_output_brief_result(const motion_fused_frame_t *frame, const motion_ai_result_t *result);
static void task1_output_rule_debug(const motion_fused_frame_t *frame, const RuleEngine *eng);
static int32_t task1_bio_value_or_invalid(int32_t value, int8_t valid);

static RuleEngine g_task1_rule_engine;
typedef struct
{
  motion_output_mode_t last_mode;
  uint8_t capture_header_printed;
  uint8_t recognition_header_printed;
  uint8_t rule_header_printed;
  uint8_t recognition_done_reported;
  uint8_t brief_last_infer_count;
  uint8_t brief_final_reported;
  uint8_t single_once_last_infer_count;
  motion_ai_state_t single_once_last_state;
} motion_output_state_t;

volatile motion_output_mode_t g_motion_output_mode =
  MOTION_OUTPUT_MODE_SELECT;
volatile uint8_t g_motion_single_armed = 0U;

static motion_output_state_t g_task1_output_state =
{
  (motion_output_mode_t)0xFF,
  0U,
  0U,
  0U,
  0U,
  0U,
  0U,
  0U,
  (motion_ai_state_t)0xFF
};

static uint8_t task1_consume_ai_restart_request(void)
{
  /* start 请求只在运动任务中消费，避免中断/云控直接重置 AI 状态机。 */
  return Motion_TakeRestartRequest();
}

static uint8_t task1_mode_uses_single_test(motion_output_mode_t mode)
{
  return (mode == MOTION_OUTPUT_MODE_SINGLE_ONCE) ? 1U : 0U;
}

static uint8_t task1_try_run_model_window_test(void)
{
  if (g_motion_output_mode != MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
  {
    return 0U;
  }

  return MotionWindowTest_RunPending();
}

static int32_t task1_encode_action_kind_value(int32_t test_value)
{
  return MotionEvents_EncodeActionKind(test_value);
}

static void task1_output_state_reset(motion_output_mode_t mode, uint8_t fresh_session)
{
  const motion_ai_result_t *result = MotionAi_GetResult();

  g_task1_output_state.last_mode = mode;
  g_task1_output_state.capture_header_printed = 0U;
  g_task1_output_state.recognition_header_printed = 0U;
  g_task1_output_state.rule_header_printed = 0U;

  if (fresh_session != 0U)
  {
    g_task1_output_state.recognition_done_reported = 0U;
    g_task1_output_state.brief_last_infer_count = 0U;
    g_task1_output_state.brief_final_reported = 0U;
    g_task1_output_state.single_once_last_infer_count = 0U;
    g_task1_output_state.single_once_last_state = (motion_ai_state_t)0xFF;
    return;
  }

  g_task1_output_state.recognition_done_reported =
    (result->test_done != 0U) ? 1U : 0U;
  g_task1_output_state.brief_last_infer_count = result->infer_count;
  g_task1_output_state.brief_final_reported =
    (result->test_done != 0U) ? 1U : 0U;
  g_task1_output_state.single_once_last_infer_count = result->infer_count;
  g_task1_output_state.single_once_last_state = result->ai_state;
}

static void task1_reset_recognition_state(void)
{
  /* start/clear、摔倒告警切换共用同一套复位流程，避免三处状态不同步。 */
  MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(g_motion_output_mode));
  MotionAi_Reset();
  RuleEngine_Init(&g_task1_rule_engine, NULL);
  task1_output_state_reset(g_motion_output_mode, 1U);
}

static uint8_t task1_try_take_fused_frame(motion_fused_frame_t *frame)
{
  return MotionSensorPipeline_TakeFusedFrame(frame);
}

static void task1_process_fused_frame(const motion_fused_frame_t *frame)
{
  const motion_ai_result_t *result = NULL;
  motion_output_mode_t current_mode;
  motion_output_mode_t previous_mode;

  if (frame == NULL)
  {
    return;
  }

  /* 统一在这里按输出模式分发，保持 AI、规则调试和采集输出的入口一致。 */
  current_mode = g_motion_output_mode;
  previous_mode = g_task1_output_state.last_mode;
  if (g_task1_output_state.last_mode != current_mode)
  {
    MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(current_mode));

    if ((previous_mode == MOTION_OUTPUT_MODE_RULE_DEBUG) ||
        (current_mode == MOTION_OUTPUT_MODE_RULE_DEBUG))
    {
      RuleEngine_Init(&g_task1_rule_engine, NULL);
    }

    if ((previous_mode != (motion_output_mode_t)0xFF) &&
        (task1_mode_uses_single_test(previous_mode) !=
         task1_mode_uses_single_test(current_mode)))
    {
      MotionAi_Reset();
      task1_output_state_reset(current_mode, 1U);
      return;
    }

    task1_output_state_reset(current_mode, 0U);
  }

  switch (current_mode)
  {
    case MOTION_OUTPUT_MODE_CAPTURE:
      task1_output_capture_csv(frame);
      break;

    case MOTION_OUTPUT_MODE_RECOGNITION_VERBOSE:
      result = MotionAi_ProcessFusedFrame(frame);
      task1_handle_local_fall_result(result);
      task1_output_recognition_csv(frame, result);
      break;

    case MOTION_OUTPUT_MODE_SINGLE_ONCE:
      if (g_motion_single_armed == 0U)
      {
        return;
      }
      result = MotionAi_ProcessFusedFrame(frame);
      task1_handle_local_fall_result(result);
      task1_output_recognition_csv(frame, result);
      break;

    case MOTION_OUTPUT_MODE_BIO_AI_BRIEF:
      result = MotionAi_ProcessFusedFrame(frame);
      task1_handle_local_fall_result(result);
      task1_output_brief_result(frame, result);
      break;

    case MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST:
      break;

    case MOTION_OUTPUT_MODE_RULE_DEBUG:
      {
        float raw[AXIS_COUNT];

        raw[AXIS_UPPER_YAW] = frame->upper_yaw;
        raw[AXIS_UPPER_PITCH] = frame->upper_pitch;
        raw[AXIS_UPPER_ROLL] = frame->upper_roll;
        raw[AXIS_FORE_YAW] = frame->fore_yaw;
        raw[AXIS_FORE_PITCH] = frame->fore_pitch;
        raw[AXIS_FORE_ROLL] = frame->fore_roll;

        RuleEngine_ProcessRaw(
          &g_task1_rule_engine,
          raw,
          (uint32_t)(frame->ts_us / 1000ULL));
        task1_output_rule_debug(frame, &g_task1_rule_engine);
      }
      break;

    default:
      task1_output_capture_csv(frame);
      break;
  }
}

static void task1_handle_local_fall_result(const motion_ai_result_t *result)
{
  if (result == NULL)
  {
    return;
  }

  if ((result->fall_local_detected != 0U) &&
      (result->fall_local_trigger_enabled != 0U))
  {
    OneNet_ActivateFallAlarm();
  }
}

static void task1_output_capture_csv(const motion_fused_frame_t *frame)
{
  int32_t bio_hr;
  int32_t bio_spo2;

  if (frame == NULL)
  {
    return;
  }

  bio_hr = task1_bio_value_or_invalid(
    frame->fore_bio.heart_rate,
    frame->fore_bio.hr_valid);
  bio_spo2 = task1_bio_value_or_invalid(
    frame->fore_bio.spo2,
    frame->fore_bio.spo2_valid);

  if (g_task1_output_state.capture_header_printed == 0U)
  {
    printf("ts_ms,upper_yaw,upper_pitch,upper_roll,fore_yaw,fore_pitch,fore_roll,hr,hr_valid,spo2,spo2_valid,ppg_fill,ppg_calc_count,ppg_pending,lost_u,lost_f,align_fail_count\r\n");
    g_task1_output_state.capture_header_printed = 1U;
  }

  printf("%llu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%ld,%d,%ld,%d,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
         (unsigned long long)(frame->ts_us / 1000ULL),
         frame->upper_yaw,
         frame->upper_pitch,
         frame->upper_roll,
         frame->fore_yaw,
         frame->fore_pitch,
         frame->fore_roll,
         (long)bio_hr,
         (int)frame->fore_bio.hr_valid,
         (long)bio_spo2,
         (int)frame->fore_bio.spo2_valid,
         (unsigned long)frame->fore_bio.ppg_fill,
         (unsigned long)frame->fore_bio.ppg_calc_count,
         (unsigned long)frame->fore_bio.ppg_pending,
         (unsigned long)frame->lost_u,
         (unsigned long)frame->lost_f,
         (unsigned long)frame->align_fail_count);
}

static void task1_output_recognition_csv(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result)
{
  if (g_motion_output_mode == MOTION_OUTPUT_MODE_SINGLE_ONCE)
  {
    task1_output_single_once_event(frame, result);
    return;
  }

  if ((frame == NULL) || (result == NULL))
  {
    return;
  }

  if (result->test_done == 0U)
  {
    g_task1_output_state.recognition_done_reported = 0U;
  }
  else if (g_task1_output_state.recognition_done_reported != 0U)
  {
    return;
  }

  if (g_task1_output_state.recognition_header_printed == 0U)
  {
    printf("ts_ms,ai_state,infer_count,smooth_ready,test_done,motion_energy,latest_label,latest_prob,top1_label,top1_prob_avg,final_label,abnormal_flag,align_fail_total,align_fail_delta,p_rest,p_elbow_flex,p_front_raise,p_side_raise,p_shoulder_raise\r\n");
    g_task1_output_state.recognition_header_printed = 1U;
  }

  printf("%llu,%s,%u,%u,%u,%.4f,%s,%.4f,%s,%.4f,%s,%u,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f\r\n",
         (unsigned long long)(frame->ts_us / 1000ULL),
         MotionAi_StateName(result->ai_state),
         (unsigned int)result->infer_count,
         (unsigned int)result->smooth_ready,
         (unsigned int)result->test_done,
         result->motion_energy,
         MotionAi_LabelName(result->latest_label),
         result->latest_prob,
         MotionAi_LabelName(result->top1_label),
         result->top1_prob_avg,
         MotionAi_LabelName(result->final_label),
         (unsigned int)result->abnormal_flag,
         (unsigned long)result->align_fail_total,
         (unsigned long)result->align_fail_delta,
         result->probs[0],
         result->probs[1],
         result->probs[2],
         result->probs[3],
         result->probs[4]);

  if (result->test_done != 0U)
  {
    printf("TEST_DONE,stable_label=%s,stable_prob=%.4f,latest_label=%s,latest_prob=%.4f,abnormal_flag=%u\r\n",
           MotionAi_LabelName(result->final_label),
           result->top1_prob_avg,
           MotionAi_LabelName(result->latest_label),
           result->latest_prob,
           (unsigned int)result->abnormal_flag);
    g_task1_output_state.recognition_done_reported = 1U;
  }
}

static void task1_output_brief_result(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result)
{
  int32_t upper_hr;
  int32_t upper_spo2;
  int32_t fore_hr;
  int32_t fore_spo2;

  if ((frame == NULL) || (result == NULL))
  {
    return;
  }

  upper_hr = task1_bio_value_or_invalid(
    frame->upper_bio.heart_rate,
    frame->upper_bio.hr_valid);
  upper_spo2 = task1_bio_value_or_invalid(
    frame->upper_bio.spo2,
    frame->upper_bio.spo2_valid);
  fore_hr = task1_bio_value_or_invalid(
    frame->fore_bio.heart_rate,
    frame->fore_bio.hr_valid);
  fore_spo2 = task1_bio_value_or_invalid(
    frame->fore_bio.spo2,
    frame->fore_bio.spo2_valid);

  if ((result->infer_count != 0U) &&
      (result->infer_count != g_task1_output_state.brief_last_infer_count))
  {
    printf("BRIEF,u_seq=%lu,f_seq=%lu,u_hr=%ld,u_spo2=%ld,f_hr=%ld,f_spo2=%ld,latest_label=%s\r\n",
           (unsigned long)frame->seq_u,
           (unsigned long)frame->seq_f,
           (long)upper_hr,
           (long)upper_spo2,
           (long)fore_hr,
           (long)fore_spo2,
           MotionAi_LabelName(result->latest_label));
  }

  g_task1_output_state.brief_last_infer_count = result->infer_count;

  if ((result->test_done != 0U) &&
      (g_task1_output_state.brief_final_reported == 0U))
  {
    printf("BRIEF_FINAL,u_seq=%lu,f_seq=%lu,u_hr=%ld,u_spo2=%ld,f_hr=%ld,f_spo2=%ld,latest_label=%s,final_label=%s\r\n",
           (unsigned long)frame->seq_u,
           (unsigned long)frame->seq_f,
           (long)upper_hr,
           (long)upper_spo2,
           (long)fore_hr,
           (long)fore_spo2,
           MotionAi_LabelName(result->latest_label),
           MotionAi_LabelName(result->final_label));
    g_task1_output_state.brief_final_reported = 1U;
  }
}

static void task1_output_single_once_event(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result)
{
  int32_t action_kind_value;
  int32_t test_value = 0;
  uint64_t ts_ms;
  uint8_t infer_changed;
  uint8_t should_update_train_display = 0U;

  if ((frame == NULL) || (result == NULL))
  {
    return;
  }

  ts_ms = frame->ts_us / 1000ULL;

  if ((result->ai_state == MOTION_AI_STATE_STATIC_WAIT) &&
      (g_task1_output_state.single_once_last_state != MOTION_AI_STATE_STATIC_WAIT))
  {
    printf("%llu,STATIC_WAIT\r\n", (unsigned long long)ts_ms);
  }
  else if ((result->ai_state == MOTION_AI_STATE_READY) &&
           (g_task1_output_state.single_once_last_state != MOTION_AI_STATE_READY))
  {
    printf("%llu,READY\r\n", (unsigned long long)ts_ms);
  }
  else if ((result->ai_state == MOTION_AI_STATE_FILL_WINDOW) &&
           (g_task1_output_state.single_once_last_state != MOTION_AI_STATE_FILL_WINDOW))
  {
    printf("%llu,FILL_WINDOW\r\n", (unsigned long long)ts_ms);
  }

  infer_changed =
    ((result->infer_count > g_task1_output_state.single_once_last_infer_count) &&
     (result->ai_state == MOTION_AI_STATE_RUNNING)) ? 1U : 0U;
  if (infer_changed != 0U)
  {
    printf("%llu,RUNNING,rest=%.4f,elbow_flex=%.4f,front_raise=%.4f,side_raise=%.4f,shoulder_raise=%.4f\r\n",
           (unsigned long long)ts_ms,
           result->probs[0],
           result->probs[1],
           result->probs[2],
           result->probs[3],
           result->probs[4]);
  }

  if ((result->test_done != 0U) && (g_task1_output_state.recognition_done_reported == 0U))
  {
    printf("%llu,TEST_DONE,rest=%.4f,elbow_flex=%.4f,front_raise=%.4f,side_raise=%.4f,shoulder_raise=%.4f,final_action=%s\r\n",
           (unsigned long long)ts_ms,
           result->avg_probs[0],
           result->avg_probs[1],
           result->avg_probs[2],
           result->avg_probs[3],
           result->avg_probs[4],
           MotionAi_LabelName(result->final_label));

    /* 识别完成后只排队上报，实际 MQTT 发送由低优先级网络任务处理。 */
    if (OneNet_TakeLastValidTestValue(&test_value) != 0U)
    {
      MotionEvents_QueueTestConfidence(test_value, result->top1_prob_avg);

      action_kind_value = task1_encode_action_kind_value(test_value);

      MotionEvents_QueueActionKind(action_kind_value);

      Debug_Printf("[MQTT] test+confidence queued test=%ld conf=%.3f\r\n",
                   (long)test_value,
                   (double)result->top1_prob_avg);
      Debug_Printf("[MQTT] action_kind queued value=%ld final_action=%s\r\n",
                   (long)action_kind_value,
                   MotionAi_LabelName(result->final_label));
    }
    else
    {
      action_kind_value = 0;
      MotionEvents_QueueActionKind(action_kind_value);

      Debug_Printf("[MQTT][WARN] no test cached at TEST_DONE, skip confidence and queue action_kind=0\r\n");
    }

    should_update_train_display =
      ((result->final_label == MOTION_LABEL_ELBOW_FLEX) ||
       (result->final_label == MOTION_LABEL_FRONT_RAISE) ||
       (result->final_label == MOTION_LABEL_SIDE_RAISE) ||
       (result->final_label == MOTION_LABEL_SHOULDER_RAISE)) &&
      (((action_kind_value / 10) % 10) != 0) ? 1U : 0U;

    if (should_update_train_display != 0U)
    {
      OneNet_UpdateTrainDisplayByAction((int32_t)result->final_label);

#if APP_UART7_IS_SCREEN && APP_SCREEN_IS_HEALTH_MONITOR
      MotionEvents_RequestTrainingPageRefresh();
#endif
    }
    else
    {
      Debug_Printf("[MQTT] TRAIN DISPLAY skip final_action=%s action_kind=%ld\r\n",
                   MotionAi_LabelName(result->final_label),
                   (long)action_kind_value);
    }

    g_task1_output_state.recognition_done_reported = 1U;
  }
  else if (result->test_done == 0U)
  {
    g_task1_output_state.recognition_done_reported = 0U;
  }

  g_task1_output_state.single_once_last_infer_count = result->infer_count;
  g_task1_output_state.single_once_last_state = result->ai_state;
}

static void task1_output_rule_debug(
  const motion_fused_frame_t *frame,
  const RuleEngine *eng)
{
  const ActionSession *session;
  const ActionResult *latched_result;
  const ActionTemplate *templates;
  const ActionResult *display_result;
  ActionType preview_action;
  AxisIndex main_axis;
  float main_amp;
  uint32_t peak_hold_ms;
  uint32_t total_time_ms;
  uint32_t template_count;

  if ((frame == NULL) || (eng == NULL))
  {
    return;
  }

  session = RuleEngine_GetSession(eng);
  latched_result = RuleEngine_GetResult(eng);
  template_count = eng->template_count;
  templates = eng->templates;
  preview_action = ACTION_UNKNOWN;
  main_axis = AXIS_UPPER_YAW;
  main_amp = 0.0f;
  peak_hold_ms = 0U;
  total_time_ms = 0U;

  if ((templates == NULL) || (template_count == 0U))
  {
    templates = Rule_GetDefaultTemplates(&template_count);
  }

  preview_action = Rule_RecognizeAction(session, templates, template_count, NULL);

  if ((latched_result != NULL) && (latched_result->valid != 0U))
  {
    display_result = latched_result;
    main_axis = latched_result->primary_axis;
    main_amp = latched_result->primary_axis_amp;
    peak_hold_ms = latched_result->peak_hold_ms;
    total_time_ms = latched_result->total_time_ms;
  }
  else
  {
    display_result = NULL;
    if (session != NULL)
    {
      main_axis = session->dominant_axis;
      main_amp = session->dominant_amp;
      peak_hold_ms = session->peak_hold_ms;
      total_time_ms = session->total_time_ms;
    }
  }

  if (g_task1_output_state.rule_header_printed == 0U)
  {
    printf("ts_ms,state,motion_energy,baseline_valid,session_active,preview_action,final_action,matched_template,match_score,score,grade,complete,timed_out,main_axis,main_axis_amp,peak_hold_ms,total_time_ms\r\n");
    g_task1_output_state.rule_header_printed = 1U;
  }

  printf("%llu,%s,%.4f,%u,%u,%s,%s,%s,%.4f,%u,%s,%u,%u,%s,%.4f,%lu,%lu\r\n",
         (unsigned long long)(frame->ts_us / 1000ULL),
         Rule_StateName(eng->state),
         eng->motion_energy,
         (unsigned int)eng->baseline_valid,
         (unsigned int)((session != NULL) ? session->active : 0U),
         Rule_ActionName(preview_action),
         Rule_ActionName((display_result != NULL) ? display_result->action : ACTION_UNKNOWN),
         ((display_result != NULL) && (display_result->matched_template_name != NULL)) ?
           display_result->matched_template_name : "unknown",
         ((display_result != NULL) && (display_result->matched_template != NULL)) ?
           display_result->match_score : -1.0f,
         (unsigned int)((display_result != NULL) ? display_result->score : 0U),
         Rule_GradeName((display_result != NULL) ? display_result->grade : RULE_GRADE_FAIL),
         (unsigned int)((display_result != NULL) ? display_result->complete : 0U),
         (unsigned int)((display_result != NULL) ? display_result->timed_out : 0U),
         Rule_AxisName(main_axis),
         main_amp,
         (unsigned long)peak_hold_ms,
         (unsigned long)total_time_ms);
}

static int32_t task1_bio_value_or_invalid(int32_t value, int8_t valid)
{
  if (valid == 0)
  {
    return -999;
  }

  return value;
}

void MotionTask_Run(void)
{
  uint8_t fall_alarm_output_mode = 0U;
  uint32_t last_warning_tick = osKernelGetTickCount();
  /* Task1 是实时链路主循环：消费融合帧，推进 AI/规则识别，产生待上报结果。 */
  MotionAi_Init();
  MotionWindowTest_Init();
  RuleEngine_Init(&g_task1_rule_engine, NULL);
  MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(g_motion_output_mode));
  task1_output_state_reset(g_motion_output_mode, 1U);
  /* 持续处理传感器帧和识别结果。 */
  for(;;)
  {
    motion_fused_frame_t fused_frame;
    uint8_t sensor_work_done = Motion_ProcessPendingPosePackets();

    if (task1_consume_ai_restart_request())
    {
      task1_reset_recognition_state();
      fall_alarm_output_mode = 0U;
      continue;
    }

    if (OneNet_IsFallAlarmActive() != 0U)
    {
      if (fall_alarm_output_mode == 0U)
      {
        task1_reset_recognition_state();
        fall_alarm_output_mode = 1U;
        last_warning_tick = osKernelGetTickCount();
      }

      if (g_motion_output_mode != MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
      {
        (void)task1_try_take_fused_frame(&fused_frame);
      }

      {
        uint32_t now_tick = osKernelGetTickCount();
        if ((now_tick - last_warning_tick) >= FALL_WARNING_INTERVAL_MS)
        {
          printf("warning\r\n");
          last_warning_tick = now_tick;
        }
      }

      (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
      continue;
    }

    if (fall_alarm_output_mode != 0U)
    {
      task1_reset_recognition_state();
      fall_alarm_output_mode = 0U;
    }

    if (task1_try_run_model_window_test())
    {
      continue;
    }

    if ((g_motion_output_mode != MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST) &&
        task1_try_take_fused_frame(&fused_frame))
    {
      task1_process_fused_frame(&fused_frame);
      sensor_work_done = 1U;
    }

    if (sensor_work_done == 0U)
    {
      (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    }
  }
}
