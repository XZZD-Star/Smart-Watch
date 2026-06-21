#include "bootloader_ota.h"

#include "crc16.h"
#include "main.h"
#include "ota_layout.h"
#include "w25q128.h"

#include <string.h>

#define BOOT_OTA_READ_CHUNK        512UL
#define BOOT_FLASH_WORD_SIZE        32UL
#define BOOT_FLASH_WORD_U32_COUNT    8UL

static int BootOTA_IsFirmwareSizeValid(uint32_t firmware_size)
{
  return ((firmware_size > 0UL) && (firmware_size <= OTA_BIN_MAX_SIZE));
}

static int BootOTA_ClearInfo(void)
{
  return W25Q128_EraseRange(OTA_INFO_ADDR, OTA_INFO_SIZE);
}

static int BootOTA_CalcW25Q128Crc(uint32_t address, uint32_t size, uint16_t *out_crc)
{
  uint8_t buffer[BOOT_OTA_READ_CHUNK];
  uint32_t offset = 0UL;
  uint16_t crc = CRC16_Init();
  int ret;

  if ((out_crc == 0) || (BootOTA_IsFirmwareSizeValid(size) == 0))
  {
    return BOOT_OTA_ERROR;
  }

  while (offset < size)
  {
    uint32_t remain = size - offset;
    uint32_t read_len = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;

    ret = W25Q128_ReadData(address + offset, buffer, read_len);
    if (ret != W25Q128_OK)
    {
      return BOOT_OTA_ERROR;
    }

    crc = CRC16_Update(crc, buffer, read_len);
    offset += read_len;
  }

  *out_crc = crc;
  return BOOT_OTA_INSTALLED;
}

static uint16_t BootOTA_CalcInternalFlashCrc(uint32_t address, uint32_t size)
{
  uint16_t crc = CRC16_Init();
  uint32_t offset = 0UL;

  while (offset < size)
  {
    uint32_t remain = size - offset;
    uint32_t chunk = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;

    crc = CRC16_Update(crc, (const uint8_t *)(address + offset), chunk);
    offset += chunk;
  }

  return crc;
}

static HAL_StatusTypeDef BootOTA_EraseFlashBank(uint32_t bank, uint32_t sector, uint32_t count)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0xFFFFFFFFUL;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = bank;
  erase.Sector = sector;
  erase.NbSectors = count;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_4;

  return HAL_FLASHEx_Erase(&erase, &sector_error);
}

static int BootOTA_EraseAppFlash(void)
{
  HAL_StatusTypeDef status;

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return BOOT_OTA_ERROR;
  }

  status = BootOTA_EraseFlashBank(FLASH_BANK_1, 1UL, 7UL);
  if (status == HAL_OK)
  {
    status = BootOTA_EraseFlashBank(FLASH_BANK_2, 0UL, 8UL);
  }

  (void)HAL_FLASH_Lock();

  return (status == HAL_OK) ? BOOT_OTA_INSTALLED : BOOT_OTA_ERROR;
}

static int BootOTA_WriteAppFlash(uint32_t firmware_size)
{
  uint8_t read_buffer[BOOT_OTA_READ_CHUNK];
  uint32_t flash_word[BOOT_FLASH_WORD_U32_COUNT];
  uint8_t *flash_bytes = (uint8_t *)flash_word;
  uint32_t written = 0UL;
  int result = BOOT_OTA_INSTALLED;

  if (BootOTA_IsFirmwareSizeValid(firmware_size) == 0)
  {
    return BOOT_OTA_ERROR;
  }

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return BOOT_OTA_ERROR;
  }

  while (written < firmware_size)
  {
    uint32_t remain = firmware_size - written;
    uint32_t read_len = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;
    uint32_t offset = 0UL;

    if (W25Q128_ReadData(OTA_BIN_ADDR + written, read_buffer, read_len) != W25Q128_OK)
    {
      result = BOOT_OTA_ERROR;
      break;
    }

    while (offset < read_len)
    {
      uint32_t copy_len = read_len - offset;

      if (copy_len > BOOT_FLASH_WORD_SIZE)
      {
        copy_len = BOOT_FLASH_WORD_SIZE;
      }

      memset(flash_bytes, 0xFF, BOOT_FLASH_WORD_SIZE);
      memcpy(flash_bytes, &read_buffer[offset], copy_len);

      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                            APP_FLASH_ADDR + written + offset,
                            (uint32_t)flash_word) != HAL_OK)
      {
        result = BOOT_OTA_ERROR;
        break;
      }

      offset += copy_len;
    }

    if (result != BOOT_OTA_INSTALLED)
    {
      break;
    }

    written += read_len;
  }

  (void)HAL_FLASH_Lock();
  return result;
}

int BootOTA_TryInstall(void)
{
  OTA_Info_t info;
  uint16_t external_crc = 0U;
  uint16_t internal_crc;

  if (W25Q128_ReadData(OTA_INFO_ADDR, (uint8_t *)&info, (uint32_t)sizeof(info)) != W25Q128_OK)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if ((info.magic != OTA_INFO_MAGIC) || (info.state != OTA_STATE_READY))
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (BootOTA_IsFirmwareSizeValid(info.firmware_size) == 0)
  {
    (void)BootOTA_ClearInfo();
    return BOOT_OTA_NO_UPDATE;
  }

  if (BootOTA_CalcW25Q128Crc(OTA_BIN_ADDR, info.firmware_size, &external_crc) != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (external_crc != info.firmware_crc16)
  {
    (void)BootOTA_ClearInfo();
    return BOOT_OTA_NO_UPDATE;
  }

  if (BootOTA_EraseAppFlash() != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_ERROR;
  }

  if (BootOTA_WriteAppFlash(info.firmware_size) != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_ERROR;
  }

  internal_crc = BootOTA_CalcInternalFlashCrc(APP_FLASH_ADDR, info.firmware_size);
  if (internal_crc != info.firmware_crc16)
  {
    return BOOT_OTA_ERROR;
  }

  if (BootOTA_ClearInfo() != W25Q128_OK)
  {
    return BOOT_OTA_ERROR;
  }

  return BOOT_OTA_INSTALLED;
}
