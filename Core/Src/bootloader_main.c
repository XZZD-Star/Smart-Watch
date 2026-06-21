/* Includes ------------------------------------------------------------------*/
#include "bootloader_ota.h"
#include "main.h"
#include "ota_layout.h"
#include "w25q128.h"
#include <stdio.h>

/* Private define ------------------------------------------------------------*/
#define DTCM_RAM_START_ADDR   0x20000000UL
#define DTCM_RAM_END_ADDR     0x2001FFFFUL
#define AXI_SRAM_START_ADDR   0x24000000UL
#define AXI_SRAM_END_ADDR     0x2407FFFFUL
#define SRAM123_START_ADDR    0x30000000UL
#define SRAM123_END_ADDR      0x30047FFFUL
#define SRAM4_START_ADDR      0x38000000UL
#define SRAM4_END_ADDR        0x3800FFFFUL

/* Private typedef -----------------------------------------------------------*/
typedef void (*app_entry_t)(void);

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_USART2_UART_Init(void);
static uint8_t Boot_AppIsValid(void);
static void Boot_DeInit(void);
static void Boot_JumpToApp(void);
static uint8_t Boot_IsRamAddress(uint32_t address);

int main(void)
{
  int ota_result;

  HAL_Init();
  SystemClock_Config();
  MX_USART2_UART_Init();
  W25Q128_Init();

  printf("BOOT LOADER\r\n");
  HAL_Delay(50);

  ota_result = BootOTA_TryInstall();
  if (ota_result == BOOT_OTA_INSTALLED)
  {
    printf("OTA INSTALLED\r\n");
    HAL_Delay(50);
    NVIC_SystemReset();
  }
  else if (ota_result == BOOT_OTA_ERROR)
  {
    while (1)
    {
      printf("OTA ERROR\r\n");
      HAL_Delay(1000);
    }
  }

  if (Boot_AppIsValid())
  {
    Boot_JumpToApp();
  }

  while (1)
  {
    printf("NO APP\r\n");
    HAL_Delay(1000);
  }
}

static uint8_t Boot_IsRamAddress(uint32_t address)
{
  if ((address >= DTCM_RAM_START_ADDR) && (address <= DTCM_RAM_END_ADDR))
  {
    return 1U;
  }

  if ((address >= AXI_SRAM_START_ADDR) && (address <= AXI_SRAM_END_ADDR))
  {
    return 1U;
  }

  if ((address >= SRAM123_START_ADDR) && (address <= SRAM123_END_ADDR))
  {
    return 1U;
  }

  if ((address >= SRAM4_START_ADDR) && (address <= SRAM4_END_ADDR))
  {
    return 1U;
  }

  return 0U;
}

static uint8_t Boot_AppIsValid(void)
{
  uint32_t app_msp = *(uint32_t *)APP_FLASH_ADDR;
  uint32_t app_reset = *(uint32_t *)(APP_FLASH_ADDR + 4UL);
  uint32_t reset_address = app_reset & ~1UL;

  if ((app_msp == 0xFFFFFFFFUL) || (app_msp == 0x00000000UL))
  {
    return 0U;
  }

  if (Boot_IsRamAddress(app_msp) == 0U)
  {
    return 0U;
  }

  if ((app_reset & 1UL) == 0UL)
  {
    return 0U;
  }

  if ((reset_address < APP_FLASH_ADDR) || (reset_address >= INTERNAL_FLASH_END))
  {
    return 0U;
  }

  return 1U;
}

static void Boot_DeInit(void)
{
  uint32_t i;

  HAL_GPIO_WritePin(W25Q128_CS_GPIO_PORT, W25Q128_CS_GPIO_PIN, GPIO_PIN_SET);
  (void)HAL_UART_DeInit(&huart2);

  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;

  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

  for (i = 0UL; i < (uint32_t)(sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }

  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0UL)
  {
    SCB_CleanInvalidateDCache();
    SCB_DisableDCache();
  }

  if ((SCB->CCR & SCB_CCR_IC_Msk) != 0UL)
  {
    SCB_InvalidateICache();
    SCB_DisableICache();
  }
}

static void Boot_JumpToApp(void)
{
  uint32_t app_msp = *(uint32_t *)APP_FLASH_ADDR;
  uint32_t app_reset = *(uint32_t *)(APP_FLASH_ADDR + 4UL);

  __disable_irq();
  Boot_DeInit();

  SCB->VTOR = APP_FLASH_ADDR;
  __set_CONTROL(0U);
  __ISB();
  __set_MSP(app_msp);
  __DSB();
  __ISB();

  ((app_entry_t)app_reset)();
}

void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (uartHandle->Instance == USART2)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
  {
  }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 240;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                              | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
