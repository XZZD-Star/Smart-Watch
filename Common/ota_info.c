#include "ota_info.h"

#include <stddef.h>
#include <string.h>

#include "crc16.h"
#include "md5.h"
#include "w25q128.h"

uint16_t OTAInfo_CalcCrc16(const OTA_Info_t *info)
{
  if (info == 0)
  {
    return 0U;
  }

  return CRC16_Calculate((const uint8_t *)info, (uint32_t)offsetof(OTA_Info_t, info_crc16));
}

uint8_t OTAInfo_IsReadyValid(const OTA_Info_t *info)
{
  if (info == 0)
  {
    return 0U;
  }

  if ((info->header_magic != OTA_INFO_MAGIC) ||
      (info->ready_flag != OTA_READY_FLAG) ||
      (info->firmware_size == 0UL) ||
      (info->firmware_size > OTA_BIN_MAX_SIZE) ||
      (MD5_IsHexString(info->expected_md5) == 0U))
  {
    return 0U;
  }

  return (OTAInfo_CalcCrc16(info) == info->info_crc16) ? 1U : 0U;
}

uint8_t OTAInfo_IsSimulateValid(const OTA_Info_t *info)
{
  if (info == 0)
  {
    return 0U;
  }

  if ((info->header_magic != OTA_INFO_MAGIC) ||
      (info->ready_flag != OTA_SIMULATE_FLAG) ||
      (info->target_version[0] == '\0'))
  {
    return 0U;
  }

  return (OTAInfo_CalcCrc16(info) == info->info_crc16) ? 1U : 0U;
}

int OTAInfo_Load(OTA_Info_t *info)
{
  if (info == 0)
  {
    return OTA_INFO_ERR;
  }

  memset(info, 0, sizeof(*info));
  if (W25Q128_ReadData(OTA_INFO_ADDR, (uint8_t *)info, (uint32_t)sizeof(*info)) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  return (OTAInfo_IsReadyValid(info) != 0U) ? OTA_INFO_OK : OTA_INFO_NO_READY;
}

int OTAInfo_LoadSimulate(OTA_Info_t *info)
{
  if (info == 0)
  {
    return OTA_INFO_ERR;
  }

  memset(info, 0, sizeof(*info));
  if (W25Q128_ReadData(OTA_INFO_ADDR, (uint8_t *)info, (uint32_t)sizeof(*info)) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  return (OTAInfo_IsSimulateValid(info) != 0U) ? OTA_INFO_OK : OTA_INFO_NO_READY;
}

int OTAInfo_SaveReady(const OTA_Info_t *info)
{
  OTA_Info_t copy;

  if (info == 0)
  {
    return OTA_INFO_ERR;
  }

  copy = *info;
  copy.header_magic = OTA_INFO_MAGIC;
  copy.ready_flag = OTA_READY_FLAG;
  copy.info_crc16 = OTAInfo_CalcCrc16(&copy);

  if (W25Q128_EraseRange(OTA_INFO_ADDR, OTA_INFO_SIZE) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  if (W25Q128_WriteDataVerified(OTA_INFO_ADDR,
                                (const uint8_t *)&copy,
                                (uint32_t)sizeof(copy)) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  return (OTAInfo_IsReadyValid(&copy) != 0U) ? OTA_INFO_OK : OTA_INFO_ERR;
}

int OTAInfo_SaveSimulate(const OTA_Info_t *info)
{
  OTA_Info_t copy;

  if (info == 0)
  {
    return OTA_INFO_ERR;
  }

  copy = *info;
  copy.header_magic = OTA_INFO_MAGIC;
  copy.ready_flag = OTA_SIMULATE_FLAG;
  copy.info_crc16 = OTAInfo_CalcCrc16(&copy);

  if (W25Q128_EraseRange(OTA_INFO_ADDR, OTA_INFO_SIZE) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  if (W25Q128_WriteDataVerified(OTA_INFO_ADDR,
                                (const uint8_t *)&copy,
                                (uint32_t)sizeof(copy)) != W25Q128_OK)
  {
    return OTA_INFO_ERR;
  }

  return (OTAInfo_IsSimulateValid(&copy) != 0U) ? OTA_INFO_OK : OTA_INFO_ERR;
}

int OTAInfo_Invalidate(void)
{
  return (W25Q128_EraseRange(OTA_INFO_ADDR, OTA_INFO_SIZE) == W25Q128_OK) ?
         OTA_INFO_OK : OTA_INFO_ERR;
}
