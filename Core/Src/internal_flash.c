#include "internal_flash.h"

#include <string.h>

#include "main.h"
#include "ota_layout.h"

#define H743_FLASH_BASE_ADDR          0x08000000UL
#define H743_FLASH_BANK_SIZE          0x00100000UL
#define H743_FLASH_SECTOR_SIZE        0x00020000UL
#define H743_FLASH_WORD_SIZE          32UL
#define H743_FLASH_WORD_U32_COUNT      8UL

static int InternalFlash_GetBankSector(uint32_t address, uint32_t *bank, uint32_t *sector)
{
  if ((bank == 0) || (sector == 0) ||
      (address < H743_FLASH_BASE_ADDR) ||
      (address >= INTERNAL_FLASH_END))
  {
    return INTERNAL_FLASH_ERR;
  }

  if (address < (H743_FLASH_BASE_ADDR + H743_FLASH_BANK_SIZE))
  {
    *bank = FLASH_BANK_1;
    *sector = (address - H743_FLASH_BASE_ADDR) / H743_FLASH_SECTOR_SIZE;
  }
  else
  {
    *bank = FLASH_BANK_2;
    *sector = (address - H743_FLASH_BASE_ADDR - H743_FLASH_BANK_SIZE) / H743_FLASH_SECTOR_SIZE;
  }

  return INTERNAL_FLASH_OK;
}

static int InternalFlash_EraseSector(uint32_t bank, uint32_t sector)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0xFFFFFFFFUL;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = bank;
  erase.Sector = sector;
  erase.NbSectors = 1UL;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_4;

  return (HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK) ?
         INTERNAL_FLASH_OK : INTERNAL_FLASH_ERR;
}

int InternalFlash_EraseAppArea(uint32_t app_start, uint32_t app_size)
{
  uint32_t address;
  uint32_t end;
  int result = INTERNAL_FLASH_OK;

  if ((app_start != APP_FLASH_ADDR) ||
      (app_size == 0UL) ||
      (app_size > APP_FLASH_SIZE) ||
      ((app_start + app_size) > INTERNAL_FLASH_END))
  {
    return INTERNAL_FLASH_ERR;
  }

  end = app_start + app_size;
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return INTERNAL_FLASH_ERR;
  }

  for (address = app_start; address < end; address += H743_FLASH_SECTOR_SIZE)
  {
    uint32_t bank;
    uint32_t sector;

    if ((InternalFlash_GetBankSector(address, &bank, &sector) != INTERNAL_FLASH_OK) ||
        (InternalFlash_EraseSector(bank, sector) != INTERNAL_FLASH_OK))
    {
      result = INTERNAL_FLASH_ERR;
      break;
    }
  }

  (void)HAL_FLASH_Lock();
  SCB_CleanInvalidateDCache();
  SCB_InvalidateICache();
  return result;
}

int InternalFlash_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
  uint32_t written = 0UL;
  uint32_t padded_length;
  int result = INTERNAL_FLASH_OK;

  padded_length = (length + H743_FLASH_WORD_SIZE - 1UL) &
                  ~(H743_FLASH_WORD_SIZE - 1UL);

  if ((data == 0) ||
      (length == 0UL) ||
      (address < APP_FLASH_ADDR) ||
      ((address + padded_length) > INTERNAL_FLASH_END))
  {
    return INTERNAL_FLASH_ERR;
  }

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return INTERNAL_FLASH_ERR;
  }

  while (written < length)
  {
    uint32_t flash_word[H743_FLASH_WORD_U32_COUNT];
    uint8_t *flash_bytes = (uint8_t *)flash_word;
    uint32_t copy_len = length - written;

    if (copy_len > H743_FLASH_WORD_SIZE)
    {
      copy_len = H743_FLASH_WORD_SIZE;
    }

    memset(flash_bytes, 0xFF, H743_FLASH_WORD_SIZE);
    memcpy(flash_bytes, &data[written], copy_len);

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                          address + written,
                          (uint32_t)flash_word) != HAL_OK)
    {
      result = INTERNAL_FLASH_ERR;
      break;
    }

    if (InternalFlash_Verify(address + written, flash_bytes, H743_FLASH_WORD_SIZE) != INTERNAL_FLASH_OK)
    {
      result = INTERNAL_FLASH_ERR;
      break;
    }

    written += copy_len;
  }

  (void)HAL_FLASH_Lock();
  SCB_CleanInvalidateDCache();
  SCB_InvalidateICache();
  return result;
}

int InternalFlash_Verify(uint32_t address, const uint8_t *data, uint32_t length)
{
  if ((data == 0) ||
      (length == 0UL) ||
      (address < APP_FLASH_ADDR) ||
      ((address + length) > INTERNAL_FLASH_END))
  {
    return INTERNAL_FLASH_ERR;
  }

  return (memcmp((const void *)address, data, length) == 0) ?
         INTERNAL_FLASH_OK : INTERNAL_FLASH_ERR;
}
