#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <stdint.h>

#include "ota_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_SERVICE_NO_UPDATE   0
#define OTA_SERVICE_UPDATED     1
#define OTA_SERVICE_ERROR      -1

typedef struct
{
  char target[OTA_TARGET_VERSION_LEN];
  char tid[OTA_TASK_ID_LEN];
  char md5[OTA_MD5_HEX_LEN + 1U];
  uint32_t size;
  uint32_t type;
  uint32_t status;
} OTAService_TaskInfo_t;

int OTAService_QueryTask(OTAService_TaskInfo_t *out_task,
                         char *out_current_version,
                         uint32_t current_version_size,
                         char *out_latest_version,
                         uint32_t latest_version_size);
int OTAService_StartSimulateUpdate(const OTAService_TaskInfo_t *task);
int OTAService_ReportPendingSimulateVersion(void);
int OTAService_CheckOnce(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_SERVICE_H */
