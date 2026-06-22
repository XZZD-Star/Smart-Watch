#include "ota_device_info.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "crc16.h"
#include "w25q128.h"

static uint16_t OTADeviceInfo_CalcCrc16(const OTA_DeviceInfo_t *info)
{
  if (info == 0)
  {
    return 0U;
  }

  return CRC16_Calculate((const uint8_t *)info,
                         (uint32_t)offsetof(OTA_DeviceInfo_t, info_crc16));
}

uint8_t OTADeviceInfo_IsValid(const OTA_DeviceInfo_t *info)
{
  if (info == 0)
  {
    return 0U;
  }

  if ((info->header_magic != OTA_DEVICE_INFO_MAGIC) ||
      (info->current_version[0] == '\0'))
  {
    return 0U;
  }

  return (OTADeviceInfo_CalcCrc16(info) == info->info_crc16) ? 1U : 0U;
}

static int OTADeviceInfo_ReadSlot(uint32_t address, OTA_DeviceInfo_t *info)
{
  if (info == 0)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  memset(info, 0, sizeof(*info));
  if (W25Q128_ReadData(address, (uint8_t *)info, (uint32_t)sizeof(*info)) != W25Q128_OK)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  return (OTADeviceInfo_IsValid(info) != 0U) ? OTA_DEVICE_INFO_OK : OTA_DEVICE_INFO_NO_VALID;
}

static int OTADeviceInfo_LoadBest(OTA_DeviceInfo_t *info, uint32_t *slot_addr)
{
  OTA_DeviceInfo_t slot_a;
  OTA_DeviceInfo_t slot_b;
  int ret_a = OTADeviceInfo_ReadSlot(OTA_DEVICE_INFO_A_ADDR, &slot_a);
  int ret_b = OTADeviceInfo_ReadSlot(OTA_DEVICE_INFO_B_ADDR, &slot_b);

  if ((ret_a == OTA_DEVICE_INFO_OK) &&
      ((ret_b != OTA_DEVICE_INFO_OK) || (slot_a.sequence >= slot_b.sequence)))
  {
    if (info != 0)
    {
      *info = slot_a;
    }
    if (slot_addr != 0)
    {
      *slot_addr = OTA_DEVICE_INFO_A_ADDR;
    }
    return OTA_DEVICE_INFO_OK;
  }

  if (ret_b == OTA_DEVICE_INFO_OK)
  {
    if (info != 0)
    {
      *info = slot_b;
    }
    if (slot_addr != 0)
    {
      *slot_addr = OTA_DEVICE_INFO_B_ADDR;
    }
    return OTA_DEVICE_INFO_OK;
  }

  if ((ret_a == OTA_DEVICE_INFO_ERR) || (ret_b == OTA_DEVICE_INFO_ERR))
  {
    return OTA_DEVICE_INFO_ERR;
  }

  return OTA_DEVICE_INFO_NO_VALID;
}

int OTADeviceInfo_Load(OTA_DeviceInfo_t *info)
{
  if (info == 0)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  memset(info, 0, sizeof(*info));
  return OTADeviceInfo_LoadBest(info, 0);
}

int OTADeviceInfo_GetCurrentVersion(char *out, uint32_t out_size)
{
  OTA_DeviceInfo_t info;

  if ((out == 0) || (out_size == 0UL))
  {
    return OTA_DEVICE_INFO_ERR;
  }

  out[0] = '\0';
  if (OTADeviceInfo_Load(&info) != OTA_DEVICE_INFO_OK)
  {
    return OTA_DEVICE_INFO_NO_VALID;
  }

  (void)snprintf(out, out_size, "%s", info.current_version);
  return OTA_DEVICE_INFO_OK;
}

static int OTADeviceInfo_WriteSlot(uint32_t address, const OTA_DeviceInfo_t *info)
{
  if (info == 0)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  if (W25Q128_EraseRange(address, OTA_DEVICE_INFO_SIZE) != W25Q128_OK)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  if (W25Q128_WriteDataVerified(address,
                                (const uint8_t *)info,
                                (uint32_t)sizeof(*info)) != W25Q128_OK)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  return OTA_DEVICE_INFO_OK;
}

int OTADeviceInfo_SaveRunningVersion(const char *version)
{
  OTA_DeviceInfo_t old_info;
  OTA_DeviceInfo_t new_info;
  uint32_t old_slot = OTA_DEVICE_INFO_B_ADDR;
  uint32_t new_slot = OTA_DEVICE_INFO_A_ADDR;
  int load_ret;

  if ((version == 0) || (*version == '\0'))
  {
    return OTA_DEVICE_INFO_ERR;
  }

  load_ret = OTADeviceInfo_LoadBest(&old_info, &old_slot);
  if ((load_ret == OTA_DEVICE_INFO_OK) &&
      (strcmp(old_info.current_version, version) == 0))
  {
    return OTA_DEVICE_INFO_OK;
  }

  memset(&new_info, 0, sizeof(new_info));
  new_info.header_magic = OTA_DEVICE_INFO_MAGIC;
  new_info.sequence = 1UL;
  new_info.last_upgrade_result = OTA_DEVICE_UPGRADE_RESULT_SUCCESS;
  (void)snprintf(new_info.current_version, sizeof(new_info.current_version), "%s", version);

  if (load_ret == OTA_DEVICE_INFO_OK)
  {
    new_info.sequence = old_info.sequence + 1UL;
    (void)snprintf(new_info.previous_version,
                   sizeof(new_info.previous_version),
                   "%s",
                   old_info.current_version);
    new_slot = (old_slot == OTA_DEVICE_INFO_A_ADDR) ?
               OTA_DEVICE_INFO_B_ADDR : OTA_DEVICE_INFO_A_ADDR;
  }
  else if (load_ret == OTA_DEVICE_INFO_ERR)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  new_info.info_crc16 = OTADeviceInfo_CalcCrc16(&new_info);
  if (OTADeviceInfo_WriteSlot(new_slot, &new_info) != OTA_DEVICE_INFO_OK)
  {
    return OTA_DEVICE_INFO_ERR;
  }

  return (OTADeviceInfo_IsValid(&new_info) != 0U) ?
         OTA_DEVICE_INFO_OK : OTA_DEVICE_INFO_ERR;
}
