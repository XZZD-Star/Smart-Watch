#ifndef OTA_DEVICE_INFO_H
#define OTA_DEVICE_INFO_H

#include <stdint.h>

#include "ota_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_DEVICE_INFO_OK        0
#define OTA_DEVICE_INFO_ERR      -1
#define OTA_DEVICE_INFO_NO_VALID  1

#define OTA_DEVICE_UPGRADE_RESULT_NONE     0UL
#define OTA_DEVICE_UPGRADE_RESULT_SUCCESS  1UL

uint8_t OTADeviceInfo_IsValid(const OTA_DeviceInfo_t *info);
int OTADeviceInfo_Load(OTA_DeviceInfo_t *info);
int OTADeviceInfo_GetCurrentVersion(char *out, uint32_t out_size);
int OTADeviceInfo_SaveRunningVersion(const char *version);

#ifdef __cplusplus
}
#endif

#endif /* OTA_DEVICE_INFO_H */
