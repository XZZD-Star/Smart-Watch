#ifndef OTA_HTTP_H
#define OTA_HTTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_HTTP_OK             0
#define OTA_HTTP_ERR           -1
#define OTA_HTTP_ERR_TIMEOUT   -2
#define OTA_HTTP_ERR_HEADER    -3
#define OTA_HTTP_ERR_LENGTH    -4

typedef struct
{
  uint16_t status_code;
  uint8_t has_content_length;
  uint8_t has_content_range;
  uint32_t content_length;
  uint32_t body_received;
  uint32_t range_start;
  uint32_t range_end;
  uint32_t range_total;
} OTAHttp_Response_t;

typedef int (*OTAHttp_BodyCallback_t)(const uint8_t *data, uint32_t length, void *user);

int OTAHttp_SendRequest(const char *request,
                        uint32_t request_length,
                        OTAHttp_Response_t *response,
                        OTAHttp_BodyCallback_t body_callback,
                        void *user);

#ifdef __cplusplus
}
#endif

#endif /* OTA_HTTP_H */
