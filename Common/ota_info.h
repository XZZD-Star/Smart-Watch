#ifndef OTA_INFO_H
#define OTA_INFO_H

#include <stdint.h>

#include "ota_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_INFO_OK         0
#define OTA_INFO_ERR       -1
#define OTA_INFO_NO_READY   1

uint16_t OTAInfo_CalcCrc16(const OTA_Info_t *info);
uint8_t OTAInfo_IsReadyValid(const OTA_Info_t *info);
int OTAInfo_Load(OTA_Info_t *info);
int OTAInfo_SaveReady(const OTA_Info_t *info);
int OTAInfo_Invalidate(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_INFO_H */
