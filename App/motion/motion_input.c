#include "motion_input.h"

#include <string.h>

#include "./usart/yuanzi_usart.h"
#include "FreeRTOS.h"
#include "main.h"
#include "task.h"
#include "motion_mode.h"
#include "motion_sensor_pipeline.h"
#include "motion_window_test.h"
#include "onenet.h"

static volatile uint8_t g_motion_ai_restart_req = 0U;
static volatile uint8_t g_motion_ai_stop_req = 0U;

static void reset_pose_pipeline(void)
{
    __disable_irq();
    MotionSensorPipeline_Reset();
    __enable_irq();

    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    memset(g_rx_buffer2, 0, sizeof(g_rx_buffer2));
}

void Motion_RequestStart(void)
{
    if (g_motion_output_mode == MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
    {
        g_motion_single_armed = 0U;
        taskENTER_CRITICAL();
        g_motion_ai_stop_req = 0U;
        taskEXIT_CRITICAL();
        MotionWindowTest_RequestRun();
        return;
    }

    if (g_motion_ai_restart_req != 0U)
    {
        return;
    }

    g_motion_single_armed =
        (g_motion_output_mode == MOTION_OUTPUT_MODE_RUN) ? 1U : 0U;

    reset_pose_pipeline();
    taskENTER_CRITICAL();
    g_motion_ai_stop_req = 0U;
    g_motion_ai_restart_req = 1U;
    taskEXIT_CRITICAL();
}

void Motion_RequestStop(void)
{
    taskENTER_CRITICAL();
    g_motion_single_armed = 0U;
    g_motion_ai_restart_req = 0U;
    g_motion_ai_stop_req = 1U;
    taskEXIT_CRITICAL();
}

void Motion_RequestClear(void)
{
    OneNet_ClearFallAlarm();
}

uint8_t Motion_TakeRestartRequest(void)
{
    uint8_t requested = 0U;

    taskENTER_CRITICAL();
    if (g_motion_ai_restart_req != 0U)
    {
        g_motion_ai_restart_req = 0U;
        requested = 1U;
    }
    taskEXIT_CRITICAL();

    return requested;
}

uint8_t Motion_TakeStopRequest(void)
{
    uint8_t requested = 0U;

    taskENTER_CRITICAL();
    if (g_motion_ai_stop_req != 0U)
    {
        g_motion_ai_stop_req = 0U;
        requested = 1U;
    }
    taskEXIT_CRITICAL();

    return requested;
}
