/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "./usart/yuanzi_usart.h"
#include "string.h"
#include <stdlib.h>   // strtof
#include <ctype.h>    // isdigit
#include <stdint.h>
#include "usart.h"
#include "motion_input.h"
#include "motion_sensor_pipeline.h"
#include "motion_mode.h"
#include "motion_window_test.h"
#include "onenet.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void reset_pose_pipeline(void);
static void motion_uart4_handle_command(const uint8_t *buf, uint16_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart7;
extern TIM_HandleTypeDef htim1;

/* USER CODE BEGIN EV */
float Calibrate_yaw,Calibrate_pitch,Calibrate_roll,Calibrate_yaw2,Calibrate_pitch2,Calibrate_roll2;
int extract_ypr(const uint8_t *buf, int len,
                float *yaw, float *pitch, float *roll);
uint16_t Calibrate_count = 0;     //校准3s计时
uint16_t startRcv = 0;            //接收校准信号标识位

volatile uint8_t  g_motion_ai_restart_req = 0U;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream0 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream1 global interrupt.
  */
void DMA1_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream1_IRQn 0 */

  /* USER CODE END DMA1_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart3_rx);
  /* USER CODE BEGIN DMA1_Stream1_IRQn 1 */

  /* USER CODE END DMA1_Stream1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream2 global interrupt.
  */
void DMA1_Stream2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream2_IRQn 0 */

  /* USER CODE END DMA1_Stream2_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_uart4_rx);
  /* USER CODE BEGIN DMA1_Stream2_IRQn 1 */

  /* USER CODE END DMA1_Stream2_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream3 global interrupt.
  */
void DMA1_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream3_IRQn 0 */

  /* USER CODE END DMA1_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart6_rx);
  /* USER CODE BEGIN DMA1_Stream3_IRQn 1 */

  /* USER CODE END DMA1_Stream3_IRQn 1 */
}

/**
  * @brief This function handles TIM1 update interrupt.
  */
void TIM1_UP_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_IRQn 0 */
    
  /* USER CODE END TIM1_UP_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_IRQn 1 */

  /* USER CODE END TIM1_UP_IRQn 1 */
}

/**
  * @brief This function handles TIM2 global interrupt.
  */
void TIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM2_IRQn 0 */
    /* Calibration disabled for data collection. */
    /*
    if (startRcv)
    {
        Calibrate_count++;
        if (Calibrate_count >= 30)
        {
            Calibrate_yaw = yaw;
            Calibrate_pitch = pitch;
            Calibrate_roll = roll;
            Calibrate_yaw2 = yaw2;
            Calibrate_pitch2 = pitch2;
            Calibrate_roll2 = roll2;
            Calibrate_count = 0;
            startRcv = 0;
        }
    }
    */
  /* USER CODE END TIM2_IRQn 0 */
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */

  /* USER CODE END TIM2_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  uint32_t tmp_flag = 0;
  uint16_t rx_len = 0;
  tmp_flag = __HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE);
  if ((tmp_flag != RESET))
  {
    rx_len = (uint16_t)(RXBUFFERSIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
    if (rx_len > RXBUFFERSIZE)
    {
      rx_len = RXBUFFERSIZE;
    }
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    HAL_UART_DMAStop(&huart1);
    MotionSensorPipeline_StorePacketFromIsr(MOTION_SENSOR_ID_UPPER, g_rx_buffer, rx_len);
    recv_end_flag = 1;
  }

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_rx_buffer, RXBUFFERSIZE);
  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */
  uint32_t tmp_flag = 0;
  uint16_t rx_len = 0;
  tmp_flag = __HAL_UART_GET_FLAG(&huart3, UART_FLAG_IDLE);
  if ((tmp_flag != RESET))
  {
    rx_len = (uint16_t)(RXBUFFERSIZE - __HAL_DMA_GET_COUNTER(&hdma_usart3_rx));
    if (rx_len > RXBUFFERSIZE)
    {
      rx_len = RXBUFFERSIZE;
    }
    __HAL_UART_CLEAR_IDLEFLAG(&huart3);
    HAL_UART_DMAStop(&huart3);
    MotionSensorPipeline_StorePacketFromIsr(MOTION_SENSOR_ID_FORE, g_rx_buffer2, rx_len);
    recv_end_flag2 = 1;
  }
  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_rx_buffer2, RXBUFFERSIZE);
  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles UART4 global interrupt.
  */
void UART4_IRQHandler(void)
{
  /* USER CODE BEGIN UART4_IRQn 0 */
    uint32_t tmp_flag = 0;
    uint16_t rx_len = 0;
   	tmp_flag =__HAL_UART_GET_FLAG(&huart4,UART_FLAG_IDLE); //��ȡIDLE��־λ
	if((tmp_flag != RESET))//idle��־����λ
	{ 
		__HAL_UART_CLEAR_IDLEFLAG(&huart4);//�����־λ
		//temp = huart1.Instance->SR;  //���״̬�Ĵ���SR,��ȡSR�Ĵ�������ʵ�����SR�Ĵ����Ĺ���
		//temp = huart1.Instance->DR; //��ȡ���ݼĴ����е�����
		//������������Ǿ��Ч
        rx_len = (uint16_t)(RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_uart4_rx));
        if (rx_len > RX_BUFFER_SIZE)
        {
            rx_len = RX_BUFFER_SIZE;
        }
        HAL_UART_DMAStop(&huart4);
        motion_uart4_handle_command(rx_buffer, rx_len);
        memset(rx_buffer, 0, sizeof(rx_buffer));
//        rx_index = sizeof(rx_buffer) - __HAL_DMA_GET_COUNTER(huart4.hdmarx);
         
		//temp  = hdma_usart1_rx.Instance->NDTR;//��ȡNDTR�Ĵ��� ��ȡDMA��δ��������ݸ�����
		//���������Ǿ��Ч
        
//        
    }
  /* USER CODE END UART4_IRQn 0 */
  HAL_UART_IRQHandler(&huart4);
  /* USER CODE BEGIN UART4_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart4,rx_buffer,RX_BUFFER_SIZE);
  /* USER CODE END UART4_IRQn 1 */
}

/**
  * @brief This function handles USART6 global interrupt.
  */
void USART6_IRQHandler(void)
{
  /* USER CODE BEGIN USART6_IRQn 0 */

  /* USER CODE END USART6_IRQn 0 */
  HAL_UART_IRQHandler(&huart6);
  /* USER CODE BEGIN USART6_IRQn 1 */

  /* USER CODE END USART6_IRQn 1 */
}

/**
  * @brief This function handles UART7 global interrupt.
  */
void UART7_IRQHandler(void)
{
  /* USER CODE BEGIN UART7_IRQn 0 */

  /* USER CODE END UART7_IRQn 0 */
  HAL_UART_IRQHandler(&huart7);
  /* USER CODE BEGIN UART7_IRQn 1 */

  /* USER CODE END UART7_IRQn 1 */
}

/* USER CODE BEGIN 1 */
static char *trim_spaces(char *s)
{
    char *end = NULL;
    while (*s != '\0' && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }
    return s;
}

int extract_ypr(const uint8_t *buf, int len,
                float *yaw, float *pitch, float *roll)
{
const char *p   = (const char *)buf;
    const char *end = p + len;

    int found = 0;

    /* �ֲ� lambda����ȡһ������ */
    #define PARSE_KEY(key, dst) do {                                    \
        const char *k = (const char *)buf;                              \
            k = strstr(p, key);                                          \
        if (k) {                                                       \
            k += strlen(key);                                          \
            char tmp[16] = {0};                                        \
            int  n = 0;                                                \
            if (k < end && *k == '-') tmp[n++] = *k++;                 \
            /* �����ֺ�С���� */                                       \
            while (k < end && (isdigit((unsigned char)*k) || *k == '.') && n < 15) \
                tmp[n++] = *k++;                                       \
            if (n > 0) {                                               \
                tmp[n] = '\0';                                         \
                *(dst) = strtof(tmp, NULL);                            \
                ++found;                                               \
            }                                                          \
        }                                                              \
    } while (0)

    /* ˳���޹صؽ��������ֶ� */
    PARSE_KEY("yaw:",   yaw);
    PARSE_KEY("pitch:", pitch);
    PARSE_KEY("roll:",  roll);

    #undef PARSE_KEY
    return found;
}

static void reset_pose_pipeline(void)
{
    __disable_irq();
    MotionSensorPipeline_Reset();
    Calibrate_count = 0U;
    __enable_irq();

    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    memset(g_rx_buffer2, 0, sizeof(g_rx_buffer2));
}

static void motion_uart4_handle_command(const uint8_t *buf, uint16_t len)
{
    char line[64] = {0};
    uint16_t i = 0U;
    uint16_t n = 0U;
    char *command = NULL;

    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    for (i = 0U; i < len && n < (sizeof(line) - 1U); i++)
    {
        char c = (char)buf[i];
        if (c == '\0')
        {
            return;
        }
        if (c == '\r' || c == '\n')
        {
            if (n == 0U)
            {
                continue;
            }
            break;
        }
        line[n++] = c;
    }
    line[n] = '\0';

    command = trim_spaces(line);
    if (*command == '\0')
    {
        return;
    }

    if (strcmp(command, "start") == 0)
    {
        Motion_RequestStart();
    }
    else if (strcmp(command, "clear") == 0)
    {
        Motion_RequestClear();
    }
}

void Motion_RequestStart(void)
{
    if (g_motion_output_mode == MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
    {
        g_motion_single_armed = 0U;
        MotionWindowTest_RequestRun();
        return;
    }

    if (g_motion_ai_restart_req != 0U)
    {
        return;
    }

    if (g_motion_output_mode == MOTION_OUTPUT_MODE_SINGLE_ONCE)
    {
        g_motion_single_armed = 1U;
    }
    else
    {
        g_motion_single_armed = 0U;
    }

    reset_pose_pipeline();
    g_motion_ai_restart_req = 1U;
}

void Motion_RequestClear(void)
{
    OneNet_ClearFallAlarm();
}

/* USER CODE END 1 */
