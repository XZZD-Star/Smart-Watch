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
#include <ctype.h>
#include <stdint.h>
#include "usart.h"
#include "motion_input.h"
#include "motion_sensor_pipeline.h"
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
    /* 传感器中断只计算 DMA 长度并存包，解析/融合交给 Task1。 */
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
    /* 前臂传感器与上臂保持同样的轻量 ISR 路径。 */
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

    /* UART4 只解析本地命令文本，真正控制动作交给 App/motion。 */
    if (strcmp(command, "start") == 0)
    {
        Motion_RequestStartFromIsr();
    }
    else if (strcmp(command, "clear") == 0)
    {
        Motion_RequestClear();
    }
}

/* USER CODE END 1 */
