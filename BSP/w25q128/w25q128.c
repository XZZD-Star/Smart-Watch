#include "w25q128.h"

#include <stdio.h>

#define W25Q128_CMD_WRITE_ENABLE        0x06U
#define W25Q128_CMD_READ_STATUS_REG1    0x05U
#define W25Q128_CMD_PAGE_PROGRAM        0x02U
#define W25Q128_CMD_READ_DATA           0x03U
#define W25Q128_CMD_READ_JEDEC_ID       0x9FU
#define W25Q128_CMD_SECTOR_ERASE_4KB    0x20U

#define W25Q128_FLASH_SIZE              0x01000000UL
#define W25Q128_SECTOR_SIZE             0x1000UL
#define W25Q128_PAGE_SIZE               256U
#define W25Q128_STATUS_BUSY             0x01U
#define W25Q128_WAIT_TIMEOUT_MS         5000U

static void W25Q128_SpiDelay(void)
{
  __NOP();
  __NOP();
  __NOP();
  __NOP();
}

static void W25Q128_DelayTiny(void)
{
  volatile uint32_t i;

  for (i = 0UL; i < 80UL; i++)
  {
    __NOP();
  }
}

static void W25Q128_WriteCS(GPIO_PinState state)
{
  HAL_GPIO_WritePin(W25Q128_CS_GPIO_PORT, W25Q128_CS_GPIO_PIN, state);
}

static void W25Q128_WriteSCK(GPIO_PinState state)
{
  HAL_GPIO_WritePin(W25Q128_SCK_GPIO_PORT, W25Q128_SCK_GPIO_PIN, state);
}

static void W25Q128_WriteMOSI(GPIO_PinState state)
{
  HAL_GPIO_WritePin(W25Q128_MOSI_GPIO_PORT, W25Q128_MOSI_GPIO_PIN, state);
}

static GPIO_PinState W25Q128_ReadMISO(void)
{
  return HAL_GPIO_ReadPin(W25Q128_MISO_GPIO_PORT, W25Q128_MISO_GPIO_PIN);
}

static void W25Q128_Start(void)
{
  W25Q128_WriteSCK(GPIO_PIN_RESET);
  W25Q128_WriteCS(GPIO_PIN_RESET);
  W25Q128_DelayTiny();
}

static void W25Q128_Stop(void)
{
  W25Q128_WriteCS(GPIO_PIN_SET);
  W25Q128_WriteSCK(GPIO_PIN_RESET);
}

static uint8_t W25Q128_SwapByte(uint8_t byte)
{
  uint8_t received = 0U;
  uint8_t mask = 0x80U;

  while (mask != 0U)
  {
    W25Q128_WriteMOSI((byte & mask) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    W25Q128_SpiDelay();

    W25Q128_WriteSCK(GPIO_PIN_SET);
    W25Q128_SpiDelay();

    if (W25Q128_ReadMISO() == GPIO_PIN_SET)
    {
      received |= mask;
    }

    W25Q128_WriteSCK(GPIO_PIN_RESET);
    W25Q128_SpiDelay();

    mask >>= 1U;
  }

  return received;
}

static void W25Q128_SendAddress(uint32_t address)
{
  W25Q128_SwapByte((uint8_t)(address >> 16));
  W25Q128_SwapByte((uint8_t)(address >> 8));
  W25Q128_SwapByte((uint8_t)address);
}

static int W25Q128_IsRangeValid(uint32_t address, uint32_t length)
{
  if (length == 0UL)
  {
    return 1;
  }

  if (address >= W25Q128_FLASH_SIZE)
  {
    return 0;
  }

  return (length <= (W25Q128_FLASH_SIZE - address));
}

static uint8_t W25Q128_ReadStatusReg1(void)
{
  uint8_t status = 0U;

  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_READ_STATUS_REG1);
  status = W25Q128_SwapByte(0xFFU);
  W25Q128_Stop();

  return status;
}

static int W25Q128_WaitBusy(void)
{
  uint32_t start_tick = HAL_GetTick();

  while ((W25Q128_ReadStatusReg1() & W25Q128_STATUS_BUSY) != 0U)
  {
    if ((HAL_GetTick() - start_tick) > W25Q128_WAIT_TIMEOUT_MS)
    {
      return W25Q128_ERR_TIMEOUT;
    }
  }

  return W25Q128_OK;
}

static int W25Q128_WriteEnable(void)
{
  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_WRITE_ENABLE);
  W25Q128_Stop();

  return W25Q128_WaitBusy();
}

void W25Q128_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  W25Q128_WriteCS(GPIO_PIN_SET);
  W25Q128_WriteSCK(GPIO_PIN_RESET);
  W25Q128_WriteMOSI(GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = W25Q128_CS_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(W25Q128_CS_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = W25Q128_SCK_GPIO_PIN;
  HAL_GPIO_Init(W25Q128_SCK_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = W25Q128_MOSI_GPIO_PIN;
  HAL_GPIO_Init(W25Q128_MOSI_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = W25Q128_MISO_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(W25Q128_MISO_GPIO_PORT, &GPIO_InitStruct);

  W25Q128_WriteCS(GPIO_PIN_SET);
  W25Q128_WriteSCK(GPIO_PIN_RESET);
  W25Q128_WriteMOSI(GPIO_PIN_RESET);
}

void W25Q128_ReadID(uint8_t *out_mid, uint8_t *out_type, uint8_t *out_capacity)
{
  uint8_t mid = 0U;
  uint8_t type = 0U;
  uint8_t capacity = 0U;

  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_READ_JEDEC_ID);
  W25Q128_DelayTiny();
  mid = W25Q128_SwapByte(0xFFU);
  type = W25Q128_SwapByte(0xFFU);
  capacity = W25Q128_SwapByte(0xFFU);
  W25Q128_Stop();

  if (out_mid != 0)
  {
    *out_mid = mid;
  }
  if (out_type != 0)
  {
    *out_type = type;
  }
  if (out_capacity != 0)
  {
    *out_capacity = capacity;
  }
}

void W25Q128_ReadIDLoopTest(void)
{
  uint8_t mid = 0U;
  uint8_t type = 0U;
  uint8_t capacity = 0U;
  uint32_t i;

  for (i = 0UL; i < 10UL; i++)
  {
    W25Q128_ReadID(&mid, &type, &capacity);
    printf("ID[%lu]: %02X %02X %02X\r\n", (unsigned long)i, mid, type, capacity);
    HAL_Delay(200U);
  }
}

int W25Q128_SectorErase4KB(uint32_t address)
{
  int ret;

  if (((address % W25Q128_SECTOR_SIZE) != 0UL) ||
      (W25Q128_IsRangeValid(address, W25Q128_SECTOR_SIZE) == 0))
  {
    return W25Q128_ERR_ERASE;
  }

  ret = W25Q128_WriteEnable();
  if (ret != W25Q128_OK)
  {
    return ret;
  }

  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_SECTOR_ERASE_4KB);
  W25Q128_SendAddress(address);
  W25Q128_Stop();

  ret = W25Q128_WaitBusy();
  if (ret != W25Q128_OK)
  {
    return ret;
  }

  return W25Q128_OK;
}

int W25Q128_PageProgram(uint32_t address, const uint8_t *data, uint16_t length)
{
  int ret;
  uint16_t i;
  uint32_t page_offset = address % W25Q128_PAGE_SIZE;

  if (length == 0U)
  {
    return W25Q128_OK;
  }

  if ((data == 0) ||
      (length > W25Q128_PAGE_SIZE) ||
      ((page_offset + length) > W25Q128_PAGE_SIZE) ||
      (W25Q128_IsRangeValid(address, length) == 0))
  {
    return W25Q128_ERR_WRITE;
  }

  ret = W25Q128_WriteEnable();
  if (ret != W25Q128_OK)
  {
    return ret;
  }

  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_PAGE_PROGRAM);
  W25Q128_SendAddress(address);
  for (i = 0U; i < length; i++)
  {
    W25Q128_SwapByte(data[i]);
  }
  W25Q128_Stop();

  ret = W25Q128_WaitBusy();
  if (ret != W25Q128_OK)
  {
    return ret;
  }

  return W25Q128_OK;
}

int W25Q128_ReadData(uint32_t address, uint8_t *data, uint32_t length)
{
  uint32_t i;

  if (length == 0UL)
  {
    return W25Q128_OK;
  }

  if ((data == 0) || (W25Q128_IsRangeValid(address, length) == 0))
  {
    return W25Q128_ERR_VERIFY;
  }

  W25Q128_Start();
  W25Q128_SwapByte(W25Q128_CMD_READ_DATA);
  W25Q128_SendAddress(address);
  for (i = 0UL; i < length; i++)
  {
    data[i] = W25Q128_SwapByte(0xFFU);
  }
  W25Q128_Stop();

  return W25Q128_OK;
}

int W25Q128_EraseRange(uint32_t address, uint32_t length)
{
  uint32_t start_sector;
  uint32_t end_address;
  uint32_t sector;
  int ret;

  if (length == 0UL)
  {
    return W25Q128_OK;
  }

  if ((address >= W25Q128_FLASH_SIZE) ||
      (length > (W25Q128_FLASH_SIZE - address)))
  {
    return W25Q128_ERR_ERASE;
  }

  start_sector = address - (address % W25Q128_SECTOR_SIZE);
  end_address = address + length;

  for (sector = start_sector; sector < end_address; sector += W25Q128_SECTOR_SIZE)
  {
    ret = W25Q128_SectorErase4KB(sector);
    if (ret != W25Q128_OK)
    {
      return ret;
    }
  }

  return W25Q128_OK;
}

int W25Q128_WriteData(uint32_t address, const uint8_t *data, uint32_t length)
{
  uint32_t written = 0UL;
  int ret;

  if (length == 0UL)
  {
    return W25Q128_OK;
  }

  if ((data == 0) || (W25Q128_IsRangeValid(address, length) == 0))
  {
    return W25Q128_ERR_WRITE;
  }

  while (written < length)
  {
    uint32_t current_address = address + written;
    uint32_t page_remain = W25Q128_PAGE_SIZE - (current_address % W25Q128_PAGE_SIZE);
    uint32_t remain = length - written;
    uint32_t chunk = (remain < page_remain) ? remain : page_remain;

    ret = W25Q128_PageProgram(current_address, &data[written], (uint16_t)chunk);
    if (ret != W25Q128_OK)
    {
      return ret;
    }

    written += chunk;
  }

  return W25Q128_OK;
}

int W25Q128_Test(void)
{
  static const uint8_t test_data[32] = {
      0x57U, 0x32U, 0x35U, 0x51U, 0x31U, 0x32U, 0x38U, 0x20U,
      0x53U, 0x4FU, 0x46U, 0x54U, 0x20U, 0x53U, 0x50U, 0x49U,
      0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU};
  uint8_t read_data[sizeof(test_data)];
  uint8_t mid = 0U;
  uint8_t type = 0U;
  uint8_t capacity = 0U;
  uint32_t i;
  int ret;

  W25Q128_ReadID(&mid, &type, &capacity);
  printf("W25Q128 ID: %02X %02X %02X\r\n", mid, type, capacity);

  if (((mid != 0xEFU) && (mid != 0x0BU)) || (capacity != 0x18U))
  {
    return W25Q128_ERR_ID;
  }

  ret = W25Q128_SectorErase4KB(W25Q128_TEST_ADDR);
  if (ret != W25Q128_OK)
  {
    return (ret == W25Q128_ERR_TIMEOUT) ? ret : W25Q128_ERR_ERASE;
  }
  printf("W25Q128 ERASE OK\r\n");

  ret = W25Q128_PageProgram(W25Q128_TEST_ADDR, test_data, (uint16_t)sizeof(test_data));
  if (ret != W25Q128_OK)
  {
    return (ret == W25Q128_ERR_TIMEOUT) ? ret : W25Q128_ERR_WRITE;
  }
  printf("W25Q128 WRITE OK\r\n");

  ret = W25Q128_ReadData(W25Q128_TEST_ADDR, read_data, (uint32_t)sizeof(read_data));
  if (ret != W25Q128_OK)
  {
    return W25Q128_ERR_VERIFY;
  }

  for (i = 0UL; i < (uint32_t)sizeof(test_data); i++)
  {
    if (read_data[i] != test_data[i])
    {
      return W25Q128_ERR_VERIFY;
    }
  }

  printf("W25Q128 VERIFY OK\r\n");
  printf("W25Q128 TEST OK\r\n");

  return W25Q128_OK;
}
