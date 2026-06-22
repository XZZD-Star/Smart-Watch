#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_SERVICE_NO_UPDATE   0
#define OTA_SERVICE_UPDATED     1
#define OTA_SERVICE_ERROR      -1

int OTAService_CheckOnce(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_SERVICE_H */
