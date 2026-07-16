#include "motion_input.h"

#include <string.h>

#include "./usart/yuanzi_usart.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "main.h"
#include "task.h"
#include "motion_mode.h"
#include "motion_sensor_pipeline.h"
#include "motion_window_test.h"
#include "onenet.h"
#include "usart.h"

extern osThreadId_t Task1Handle;

static volatile uint8_t g_motion_ai_restart_req = 0U;
static volatile uint8_t g_motion_ai_stop_req = 0U;
static volatile uint8_t g_motion_start_from_isr_req = 0U;
static volatile uint8_t g_motion_upper_capture_reset_req = 0U;

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
    uint8_t should_arm;

    if (g_motion_output_mode == MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
    {
        g_motion_single_armed = 0U;
        taskENTER_CRITICAL();
        g_motion_ai_stop_req = 0U;
        taskEXIT_CRITICAL();
        MotionWindowTest_RequestRun();
        return;
    }

    should_arm =
        ((g_motion_output_mode == MOTION_OUTPUT_MODE_RUN) ||
         (g_motion_output_mode == MOTION_OUTPUT_MODE_RULE_DEBUG)) ? 1U : 0U;
    g_motion_single_armed = should_arm;

    if (g_motion_ai_restart_req != 0U)
    {
        taskENTER_CRITICAL();
        g_motion_ai_stop_req = 0U;
        taskEXIT_CRITICAL();
        return;
    }

    reset_pose_pipeline();
    taskENTER_CRITICAL();
    g_motion_ai_stop_req = 0U;
    g_motion_ai_restart_req = 1U;
    taskEXIT_CRITICAL();
}

void Motion_RequestStartFromIsr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (g_motion_output_mode == MOTION_OUTPUT_MODE_UPPER_CAPTURE)
    {
        g_motion_upper_capture_reset_req = 1U;
    }
    else
    {
        g_motion_start_from_isr_req = 1U;
    }
    if (Task1Handle != NULL)
    {
        vTaskNotifyGiveFromISR(
            (TaskHandle_t)Task1Handle,
            &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
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

uint8_t Motion_TakeStartFromIsrRequest(void)
{
    uint8_t requested = 0U;

    taskENTER_CRITICAL();
    if (g_motion_start_from_isr_req != 0U)
    {
        g_motion_start_from_isr_req = 0U;
        requested = 1U;
    }
    taskEXIT_CRITICAL();

    return requested;
}

uint8_t Motion_TakeUpperCaptureResetRequest(void)
{
    uint8_t requested = 0U;

    taskENTER_CRITICAL();
    if (g_motion_upper_capture_reset_req != 0U)
    {
        g_motion_upper_capture_reset_req = 0U;
        requested = 1U;
    }
    taskEXIT_CRITICAL();

    return requested;
}

void Motion_ResetUpperCaptureInput(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    (void)HAL_UART_DMAStop(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    MotionSensorPipeline_ResetUpperCapture();
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    recv_end_flag = 0U;
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_rx_buffer, RXBUFFERSIZE);

    if (primask == 0U)
    {
        __enable_irq();
    }
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
