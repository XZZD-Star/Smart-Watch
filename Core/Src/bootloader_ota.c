#include "bootloader_ota.h"

#include <string.h>

#include "internal_flash.h"
#include "md5.h"
#include "ota_info.h"
#include "ota_layout.h"
#include "w25q128.h"

#define BOOT_OTA_READ_CHUNK  512UL

static int BootOTA_IsFirmwareSizeValid(uint32_t firmware_size)
{
  return ((firmware_size > 0UL) && (firmware_size <= OTA_BIN_MAX_SIZE));
}

static int BootOTA_CalcW25Q128Md5(uint32_t address,
                                  uint32_t size,
                                  char out_md5[33])
{
  uint8_t buffer[BOOT_OTA_READ_CHUNK];
  uint8_t digest[16];
  uint32_t offset = 0UL;
  MD5_Context_t md5;

  if ((out_md5 == 0) || (BootOTA_IsFirmwareSizeValid(size) == 0))
  {
    return BOOT_OTA_ERROR;
  }

  MD5_Init(&md5);
  while (offset < size)
  {
    uint32_t remain = size - offset;
    uint32_t read_len = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;

    if (W25Q128_ReadData(address + offset, buffer, read_len) != W25Q128_OK)
    {
      return BOOT_OTA_ERROR;
    }

    MD5_Update(&md5, buffer, read_len);
    offset += read_len;
  }

  MD5_Final(&md5, digest);
  MD5_ToHex(digest, out_md5);
  return BOOT_OTA_INSTALLED;
}

static int BootOTA_CalcInternalFlashMd5(uint32_t address,
                                        uint32_t size,
                                        char out_md5[33])
{
  uint8_t digest[16];
  uint32_t offset = 0UL;
  MD5_Context_t md5;

  if ((out_md5 == 0) || (BootOTA_IsFirmwareSizeValid(size) == 0))
  {
    return BOOT_OTA_ERROR;
  }

  MD5_Init(&md5);
  while (offset < size)
  {
    uint32_t remain = size - offset;
    uint32_t read_len = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;

    MD5_Update(&md5, (const uint8_t *)(address + offset), read_len);
    offset += read_len;
  }

  MD5_Final(&md5, digest);
  MD5_ToHex(digest, out_md5);
  return BOOT_OTA_INSTALLED;
}

static int BootOTA_WriteAppFlash(uint32_t firmware_size)
{
  uint8_t read_buffer[BOOT_OTA_READ_CHUNK];
  uint32_t written = 0UL;

  if (BootOTA_IsFirmwareSizeValid(firmware_size) == 0)
  {
    return BOOT_OTA_ERROR;
  }

  while (written < firmware_size)
  {
    uint32_t remain = firmware_size - written;
    uint32_t read_len = (remain > BOOT_OTA_READ_CHUNK) ? BOOT_OTA_READ_CHUNK : remain;

    if (W25Q128_ReadData(OTA_BIN_ADDR + written, read_buffer, read_len) != W25Q128_OK)
    {
      return BOOT_OTA_ERROR;
    }

    if (InternalFlash_Write(APP_FLASH_ADDR + written, read_buffer, read_len) != INTERNAL_FLASH_OK)
    {
      return BOOT_OTA_ERROR;
    }

    written += read_len;
  }

  return BOOT_OTA_INSTALLED;
}

int BootOTA_TryInstall(void)
{
  OTA_Info_t info;
  char external_md5[33];
  char internal_md5[33];

  if (OTAInfo_LoadSimulate(&info) == OTA_INFO_OK)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (OTAInfo_Load(&info) != OTA_INFO_OK)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (BootOTA_IsFirmwareSizeValid(info.firmware_size) == 0)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  memset(external_md5, 0, sizeof(external_md5));
  if (BootOTA_CalcW25Q128Md5(OTA_BIN_ADDR, info.firmware_size, external_md5) != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (strcmp(external_md5, info.expected_md5) != 0)
  {
    return BOOT_OTA_NO_UPDATE;
  }

  if (InternalFlash_EraseAppArea(APP_FLASH_ADDR, APP_FLASH_SIZE) != INTERNAL_FLASH_OK)
  {
    return BOOT_OTA_ERROR;
  }

  if (BootOTA_WriteAppFlash(info.firmware_size) != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_ERROR;
  }

  memset(internal_md5, 0, sizeof(internal_md5));
  if (BootOTA_CalcInternalFlashMd5(APP_FLASH_ADDR, info.firmware_size, internal_md5) != BOOT_OTA_INSTALLED)
  {
    return BOOT_OTA_ERROR;
  }

  if (strcmp(internal_md5, info.expected_md5) != 0)
  {
    return BOOT_OTA_ERROR;
  }

  if (OTAInfo_Invalidate() != OTA_INFO_OK)
  {
    return BOOT_OTA_ERROR;
  }

  return BOOT_OTA_INSTALLED;
}
