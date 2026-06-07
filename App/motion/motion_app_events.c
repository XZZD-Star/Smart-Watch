#include "motion_app_events.h"

#include "FreeRTOS.h"
#include "task.h"

static volatile uint8_t g_test_conf_post_pending = 0U;
static volatile int32_t g_test_conf_post_value = 0;
static volatile float g_test_conf_post_confidence = 0.0f;
static volatile uint8_t g_action_kind_post_pending = 0U;
static volatile int32_t g_action_kind_post_value = 0;
static volatile uint8_t g_action_kind_toggle = 0U;
static volatile uint8_t g_screen_training_page_refresh_pending = 0U;
static volatile int32_t g_screen_training_page_refresh_action_label = 0;
static volatile uint32_t g_screen_training_page_refresh_generation = 0U;

void MotionEvents_QueueTestConfidence(int32_t test_value, float confidence)
{
  taskENTER_CRITICAL();
  g_test_conf_post_value = test_value;
  g_test_conf_post_confidence = confidence;
  g_test_conf_post_pending = 1U;
  taskEXIT_CRITICAL();
}

uint8_t MotionEvents_PeekTestConfidence(int32_t *test_value, float *confidence)
{
  uint8_t has_pending = 0U;

  if ((test_value == NULL) || (confidence == NULL))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  if (g_test_conf_post_pending != 0U)
  {
    *test_value = g_test_conf_post_value;
    *confidence = g_test_conf_post_confidence;
    has_pending = 1U;
  }
  taskEXIT_CRITICAL();

  return has_pending;
}

void MotionEvents_ClearTestConfidenceIfCurrent(int32_t test_value)
{
  taskENTER_CRITICAL();
  if ((g_test_conf_post_pending != 0U) &&
      (g_test_conf_post_value == test_value))
  {
    g_test_conf_post_pending = 0U;
  }
  taskEXIT_CRITICAL();
}

int32_t MotionEvents_EncodeActionKind(int32_t test_value)
{
  int32_t encoded_value = 0;

  if (test_value <= 0)
  {
    return 0;
  }

  taskENTER_CRITICAL();
  g_action_kind_toggle ^= 0x01U;
  encoded_value = test_value * 10 + (int32_t)(g_action_kind_toggle & 0x01U);
  taskEXIT_CRITICAL();

  return encoded_value;
}

void MotionEvents_QueueActionKind(int32_t action_kind_value)
{
  taskENTER_CRITICAL();
  g_action_kind_post_value = action_kind_value;
  g_action_kind_post_pending = 1U;
  taskEXIT_CRITICAL();
}

uint8_t MotionEvents_PeekActionKind(int32_t *action_kind_value)
{
  uint8_t has_pending = 0U;

  if (action_kind_value == NULL)
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  if (g_action_kind_post_pending != 0U)
  {
    *action_kind_value = g_action_kind_post_value;
    has_pending = 1U;
  }
  taskEXIT_CRITICAL();

  return has_pending;
}

void MotionEvents_ClearActionKindIfCurrent(int32_t action_kind_value)
{
  taskENTER_CRITICAL();
  if ((g_action_kind_post_pending != 0U) &&
      (g_action_kind_post_value == action_kind_value))
  {
    g_action_kind_post_pending = 0U;
  }
  taskEXIT_CRITICAL();
}

uint8_t MotionEvents_HasPendingCloudPost(void)
{
  uint8_t has_pending = 0U;

  taskENTER_CRITICAL();
  has_pending =
    ((g_test_conf_post_pending != 0U) ||
     (g_action_kind_post_pending != 0U)) ? 1U : 0U;
  taskEXIT_CRITICAL();

  return has_pending;
}

void MotionEvents_RequestTrainingPageRefresh(void)
{
  MotionEvents_RequestTrainingPageRefreshByAction(0);
}

void MotionEvents_RequestTrainingPageRefreshByAction(int32_t action_label)
{
  taskENTER_CRITICAL();
  g_screen_training_page_refresh_action_label = action_label;
  g_screen_training_page_refresh_pending = 1U;
  g_screen_training_page_refresh_generation++;
  taskEXIT_CRITICAL();
}

uint8_t MotionEvents_TakeTrainingPageRefresh(void)
{
  return MotionEvents_TakeTrainingPageRefreshAction(NULL);
}

uint8_t MotionEvents_TakeTrainingPageRefreshAction(int32_t *action_label)
{
  uint8_t requested = 0U;

  taskENTER_CRITICAL();
  if (g_screen_training_page_refresh_pending != 0U)
  {
    if (action_label != NULL)
    {
      *action_label = g_screen_training_page_refresh_action_label;
    }
    g_screen_training_page_refresh_pending = 0U;
    requested = 1U;
  }
  taskEXIT_CRITICAL();

  return requested;
}

uint8_t MotionEvents_PeekTrainingPageRefreshAction(int32_t *action_label)
{
  return MotionEvents_PeekTrainingPageRefreshDirty(action_label, NULL);
}

uint8_t MotionEvents_PeekTrainingPageRefreshDirty(int32_t *action_label,
                                                  uint32_t *generation)
{
  uint8_t requested = 0U;

  taskENTER_CRITICAL();
  if (g_screen_training_page_refresh_pending != 0U)
  {
    if (action_label != NULL)
    {
      *action_label = g_screen_training_page_refresh_action_label;
    }
    if (generation != NULL)
    {
      *generation = g_screen_training_page_refresh_generation;
    }
    requested = 1U;
  }
  taskEXIT_CRITICAL();

  return requested;
}

void MotionEvents_ClearTrainingPageRefresh(void)
{
  taskENTER_CRITICAL();
  g_screen_training_page_refresh_pending = 0U;
  g_screen_training_page_refresh_action_label = 0;
  taskEXIT_CRITICAL();
}

void MotionEvents_ClearTrainingPageRefreshIfCurrent(uint32_t generation)
{
  taskENTER_CRITICAL();
  if ((g_screen_training_page_refresh_pending != 0U) &&
      (g_screen_training_page_refresh_generation == generation))
  {
    g_screen_training_page_refresh_pending = 0U;
    g_screen_training_page_refresh_action_label = 0;
  }
  taskEXIT_CRITICAL();
}
