#include "ota_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ESP8266.h"
#include "cmsis_os2.h"
#include "debug_uart7.h"
#include "main.h"
#include "md5.h"
#include "ota_config.h"
#include "ota_device_info.h"
#include "ota_http.h"
#include "ota_info.h"
#include "w25q128.h"

#define OTA_HTTP_REQ_MAX       640U
#define OTA_JSON_BODY_MAX      768U
#define OTA_MSG_MAX             32U
#define OTA_SIZE_LIMIT          OTA_BIN_MAX_SIZE
#define OTA_SIMULATE_UPGRADE_ONLY 1U

typedef struct
{
  char target[OTA_TARGET_VERSION_LEN];
  char tid[OTA_TASK_ID_LEN];
  char md5[OTA_MD5_HEX_LEN + 1U];
  uint32_t size;
  uint32_t type;
  uint32_t status;
} OTA_TaskInfo_t;

typedef struct
{
  char *buffer;
  uint32_t capacity;
  uint32_t length;
} OTA_JsonBody_t;

typedef struct
{
  uint32_t write_addr;
  uint32_t expected_len;
  uint32_t received_len;
  MD5_Context_t *md5;
} OTA_DownloadBody_t;

static int OTAService_CopyJsonBody(const uint8_t *data, uint32_t length, void *user)
{
  OTA_JsonBody_t *body = (OTA_JsonBody_t *)user;

  if ((body == 0) || (body->buffer == 0) || (body->capacity == 0UL))
  {
    return OTA_HTTP_ERR;
  }

  if ((body->length + length) >= body->capacity)
  {
    return OTA_HTTP_ERR_LENGTH;
  }

  memcpy(&body->buffer[body->length], data, length);
  body->length += length;
  body->buffer[body->length] = '\0';
  return OTA_HTTP_OK;
}

static int OTAService_WriteFirmwareBody(const uint8_t *data, uint32_t length, void *user)
{
  OTA_DownloadBody_t *body = (OTA_DownloadBody_t *)user;

  if ((body == 0) || (data == 0))
  {
    return OTA_HTTP_ERR;
  }

  if ((body->received_len + length) > body->expected_len)
  {
    return OTA_HTTP_ERR_LENGTH;
  }

  if (W25Q128_WriteDataVerified(body->write_addr + body->received_len,
                                data,
                                length) != W25Q128_OK)
  {
    return OTA_HTTP_ERR;
  }

  MD5_Update(body->md5, data, length);
  body->received_len += length;
  return OTA_HTTP_OK;
}

static const char *OTAService_SkipJsonSpaces(const char *text)
{
  while ((text != 0) &&
         ((*text == ' ') || (*text == '\t') || (*text == '\r') || (*text == '\n')))
  {
    text++;
  }

  return text;
}

static const char *OTAService_FindJsonValue(const char *json, const char *key)
{
  char pattern[40];
  const char *match;
  const char *colon;
  int len;

  if ((json == 0) || (key == 0))
  {
    return 0;
  }

  len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if ((len <= 0) || ((size_t)len >= sizeof(pattern)))
  {
    return 0;
  }

  match = strstr(json, pattern);
  if (match == 0)
  {
    return 0;
  }

  colon = strchr(match + len, ':');
  if (colon == 0)
  {
    return 0;
  }

  return OTAService_SkipJsonSpaces(colon + 1);
}

static const char *OTAService_FindJsonObject(const char *json, const char *key)
{
  const char *value = OTAService_FindJsonValue(json, key);

  if ((value == 0) || (*value != '{'))
  {
    return 0;
  }

  return value;
}

static int OTAService_ParseJsonText(const char *value_start, char *out, uint32_t out_size)
{
  const char *begin;
  const char *end;
  uint32_t length;

  if ((value_start == 0) || (out == 0) || (out_size == 0UL))
  {
    return 0;
  }

  if (*value_start == '"')
  {
    begin = value_start + 1;
    end = begin;
    while ((*end != '\0') && (*end != '"'))
    {
      end++;
    }
    if (*end != '"')
    {
      return 0;
    }
  }
  else
  {
    begin = value_start;
    end = begin;
    while ((*end != '\0') &&
           (*end != ',') &&
           (*end != '}') &&
           (*end != ' ') &&
           (*end != '\r') &&
           (*end != '\n') &&
           (*end != '\t'))
    {
      end++;
    }
  }

  length = (uint32_t)(end - begin);
  if ((length == 0UL) || (length >= out_size))
  {
    return 0;
  }

  memcpy(out, begin, length);
  out[length] = '\0';
  return 1;
}

static int OTAService_ParseJsonUint(const char *json, const char *key, uint32_t *out)
{
  char text[24];
  char *end = 0;
  unsigned long value;

  if ((out == 0) ||
      (OTAService_ParseJsonText(OTAService_FindJsonValue(json, key),
                                text,
                                (uint32_t)sizeof(text)) == 0))
  {
    return 0;
  }

  value = strtoul(text, &end, 10);
  if ((end == 0) || (*end != '\0'))
  {
    return 0;
  }

  *out = (uint32_t)value;
  return 1;
}

static void OTAService_ToLowerMd5(char md5[OTA_MD5_HEX_LEN + 1U])
{
  uint32_t i;

  for (i = 0UL; i < OTA_MD5_HEX_LEN; i++)
  {
    if ((md5[i] >= 'A') && (md5[i] <= 'F'))
    {
      md5[i] = (char)(md5[i] - 'A' + 'a');
    }
  }
}

static int OTAService_ParseOnenetEnvelope(const char *json, uint32_t *code, char *msg, uint32_t msg_size)
{
  if ((json == 0) || (code == 0) || (msg == 0) ||
      (OTAService_ParseJsonUint(json, "code", code) == 0) ||
      (OTAService_ParseJsonText(OTAService_FindJsonValue(json, "msg"), msg, msg_size) == 0))
  {
    return 0;
  }

  return 1;
}

static void OTAService_SyncRunningVersion(void)
{
  if (OTADeviceInfo_SaveRunningVersion(OTA_CURRENT_VERSION) != OTA_DEVICE_INFO_OK)
  {
    Debug_Printf("[OTA] device version sync failed\r\n");
  }
}

static void OTAService_GetReportVersion(char *version, uint32_t version_size)
{
  if ((version == 0) || (version_size == 0UL))
  {
    return;
  }

  if (OTADeviceInfo_GetCurrentVersion(version, version_size) != OTA_DEVICE_INFO_OK)
  {
    (void)snprintf(version, version_size, "%s", OTA_CURRENT_VERSION);
  }
}

static int OTAService_ParseTask(const char *json, OTA_TaskInfo_t *task)
{
  if ((json == 0) || (task == 0))
  {
    return 0;
  }

  memset(task, 0, sizeof(*task));
  if ((OTAService_ParseJsonText(OTAService_FindJsonValue(json, "target"),
                                task->target,
                                (uint32_t)sizeof(task->target)) == 0) ||
      (OTAService_ParseJsonText(OTAService_FindJsonValue(json, "tid"),
                                task->tid,
                                (uint32_t)sizeof(task->tid)) == 0) ||
      (OTAService_ParseJsonUint(json, "size", &task->size) == 0) ||
      (OTAService_ParseJsonText(OTAService_FindJsonValue(json, "md5"),
                                task->md5,
                                (uint32_t)sizeof(task->md5)) == 0) ||
      (OTAService_ParseJsonUint(json, "type", &task->type) == 0) ||
      (OTAService_ParseJsonUint(json, "status", &task->status) == 0))
  {
    return 0;
  }

  OTAService_ToLowerMd5(task->md5);
  if ((MD5_IsHexString(task->md5) == 0U) ||
      (task->type != OTA_TASK_RESPONSE_TYPE) ||
      (task->status == 0UL) ||
      (task->size == 0UL) ||
      (task->size > OTA_SIZE_LIMIT))
  {
    return 0;
  }

  return 1;
}

static int OTAService_PostVersion(const char *version)
{
  char request[OTA_HTTP_REQ_MAX];
  char body[OTA_JSON_BODY_MAX];
  char json[96];
  char msg[OTA_MSG_MAX];
  OTAHttp_Response_t response;
  OTA_JsonBody_t json_body = {body, (uint32_t)sizeof(body), 0UL};
  uint32_t code = 0UL;
  int json_len;
  int request_len;

  memset(body, 0, sizeof(body));
  if ((version == 0) || (*version == '\0'))
  {
    return OTA_SERVICE_ERROR;
  }

  json_len = snprintf(json,
                      sizeof(json),
                      "{\"s_version\":\"%s\",\"f_version\":\"%s\"}",
                      version,
                      version);
  if ((json_len <= 0) || ((size_t)json_len >= sizeof(json)))
  {
    return OTA_SERVICE_ERROR;
  }

  request_len = snprintf(request,
                         sizeof(request),
                         "POST /fuse-ota/%s/%s/version HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Authorization: %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %u\r\n"
                         "Connection: keep-alive\r\n"
                         "\r\n"
                         "%s",
                         ONENET_PRODUCT_ID,
                         ONENET_DEVICE_NAME,
                         OTA_HTTP_HOST,
                         OTA_AUTHORIZATION,
                         (unsigned int)json_len,
                         json);
  if ((request_len <= 0) || ((size_t)request_len >= sizeof(request)))
  {
    return OTA_SERVICE_ERROR;
  }

  if ((OTAHttp_SendRequest(request,
                           (uint32_t)request_len,
                           &response,
                           OTAService_CopyJsonBody,
                           &json_body) != OTA_HTTP_OK) ||
      (response.status_code != 200U) ||
      (OTAService_ParseOnenetEnvelope(body, &code, msg, (uint32_t)sizeof(msg)) == 0) ||
      (code != 0UL))
  {
    Debug_Printf("[OTA] version report failed\r\n");
    return OTA_SERVICE_ERROR;
  }

  Debug_Printf("[OTA] version reported version=%s\r\n", version);
  return OTA_SERVICE_NO_UPDATE;
}

static int OTAService_PostPendingSimulateVersion(void)
{
  OTA_Info_t info;
  int result;

  result = OTAInfo_LoadSimulate(&info);
  if (result == OTA_INFO_NO_READY)
  {
    return OTA_SERVICE_NO_UPDATE;
  }
  if (result != OTA_INFO_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  if (OTAService_PostVersion(info.target_version) != OTA_SERVICE_NO_UPDATE)
  {
    return OTA_SERVICE_ERROR;
  }

  if (OTAInfo_Invalidate() != OTA_INFO_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  Debug_Printf("[OTA] simulate version reported target=%s\r\n", info.target_version);
  return OTA_SERVICE_UPDATED;
}

static int OTAService_QueryTask(OTA_TaskInfo_t *task, const char *current_version)
{
  char request[OTA_HTTP_REQ_MAX];
  char body[OTA_JSON_BODY_MAX];
  char msg[OTA_MSG_MAX];
  OTAHttp_Response_t response;
  OTA_JsonBody_t json_body = {body, (uint32_t)sizeof(body), 0UL};
  uint32_t code = 0UL;
  int request_len;

  if ((task == 0) || (current_version == 0) || (*current_version == '\0'))
  {
    return OTA_SERVICE_ERROR;
  }

  memset(body, 0, sizeof(body));
  request_len = snprintf(request,
                         sizeof(request),
                         "GET /fuse-ota/%s/%s/check?type=%u&version=%s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Authorization: %s\r\n"
                         "Connection: keep-alive\r\n"
                         "\r\n",
                         ONENET_PRODUCT_ID,
                         ONENET_DEVICE_NAME,
                         (unsigned int)OTA_QUERY_TYPE,
                         current_version,
                         OTA_HTTP_HOST,
                         OTA_AUTHORIZATION);
  if ((request_len <= 0) || ((size_t)request_len >= sizeof(request)))
  {
    return OTA_SERVICE_ERROR;
  }

  if ((OTAHttp_SendRequest(request,
                           (uint32_t)request_len,
                           &response,
                           OTAService_CopyJsonBody,
                           &json_body) != OTA_HTTP_OK) ||
      (response.status_code != 200U) ||
      (OTAService_ParseOnenetEnvelope(body, &code, msg, (uint32_t)sizeof(msg)) == 0))
  {
    Debug_Printf("[OTA] task query failed\r\n");
    return OTA_SERVICE_ERROR;
  }

  Debug_Printf("[OTA] query body=%s\r\n", body);

  if (strcmp(msg, "not exist") == 0)
  {
    Debug_Printf("[OTA] no task msg=%s code=%lu\r\n", msg, (unsigned long)code);
    return OTA_SERVICE_NO_UPDATE;
  }

  if (code != 0UL)
  {
    Debug_Printf("[OTA] task query code=%lu msg=%s\r\n", (unsigned long)code, msg);
    return OTA_SERVICE_ERROR;
  }

  if ((strcmp(msg, "succ") != 0) ||
      (OTAService_ParseTask(OTAService_FindJsonObject(body, "data"), task) == 0))
  {
    Debug_Printf("[OTA] task invalid msg=%s code=%lu\r\n", msg, (unsigned long)code);
    return OTA_SERVICE_ERROR;
  }

  Debug_Printf("[OTA] task target=%s tid=%s size=%lu type=%lu status=%lu\r\n",
               task->target,
               task->tid,
               (unsigned long)task->size,
               (unsigned long)task->type,
               (unsigned long)task->status);
  return OTA_SERVICE_UPDATED;
}

static int OTAService_DownloadRange(const OTA_TaskInfo_t *task,
                                    uint32_t offset,
                                    uint32_t chunk_len,
                                    MD5_Context_t *md5)
{
  char request[OTA_HTTP_REQ_MAX];
  OTAHttp_Response_t response;
  OTA_DownloadBody_t body;
  uint32_t end = offset + chunk_len - 1UL;
  int request_len;

  request_len = snprintf(request,
                         sizeof(request),
                         "GET /fuse-ota/%s/%s/%s/download HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Authorization: %s\r\n"
                         "Range: bytes=%lu-%lu\r\n"
                         "Connection: keep-alive\r\n"
                         "\r\n",
                         ONENET_PRODUCT_ID,
                         ONENET_DEVICE_NAME,
                         task->tid,
                         OTA_HTTP_HOST,
                         OTA_AUTHORIZATION,
                         (unsigned long)offset,
                         (unsigned long)end);
  if ((request_len <= 0) || ((size_t)request_len >= sizeof(request)))
  {
    return OTA_SERVICE_ERROR;
  }

  body.write_addr = OTA_BIN_ADDR + offset;
  body.expected_len = chunk_len;
  body.received_len = 0UL;
  body.md5 = md5;

  if (OTAHttp_SendRequest(request,
                          (uint32_t)request_len,
                          &response,
                          OTAService_WriteFirmwareBody,
                          &body) != OTA_HTTP_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  if ((response.status_code != 206U) ||
      (response.has_content_length == 0U) ||
      (response.content_length != chunk_len) ||
      (response.body_received != chunk_len) ||
      (response.has_content_range == 0U) ||
      (response.range_start != offset) ||
      (response.range_end != end) ||
      (response.range_total != task->size) ||
      (body.received_len != chunk_len))
  {
    return OTA_SERVICE_ERROR;
  }

  return OTA_SERVICE_NO_UPDATE;
}

static int OTAService_DownloadFirmware(const OTA_TaskInfo_t *task, char out_md5[33])
{
  MD5_Context_t md5;
  uint8_t digest[16];
  uint32_t erase_len;
  uint32_t offset = 0UL;

  if ((task == 0) || (out_md5 == 0))
  {
    return OTA_SERVICE_ERROR;
  }

  if (OTAInfo_Invalidate() != OTA_INFO_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  erase_len = (task->size + OTA_INFO_SIZE - 1UL) & ~(OTA_INFO_SIZE - 1UL);
  if (W25Q128_EraseRange(OTA_BIN_ADDR, erase_len) != W25Q128_OK)
  {
    Debug_Printf("[OTA] firmware slot erase failed len=%lu\r\n", (unsigned long)erase_len);
    return OTA_SERVICE_ERROR;
  }

  MD5_Init(&md5);
  while (offset < task->size)
  {
    uint32_t remain = task->size - offset;
    uint32_t chunk_len = (remain > OTA_DOWNLOAD_CHUNK_SIZE) ? OTA_DOWNLOAD_CHUNK_SIZE : remain;

    if (OTAService_DownloadRange(task, offset, chunk_len, &md5) != OTA_SERVICE_NO_UPDATE)
    {
      Debug_Printf("[OTA] download failed offset=%lu len=%lu\r\n",
                   (unsigned long)offset,
                   (unsigned long)chunk_len);
      return OTA_SERVICE_ERROR;
    }

    offset += chunk_len;
  }

  MD5_Final(&md5, digest);
  MD5_ToHex(digest, out_md5);
  return (offset == task->size) ? OTA_SERVICE_UPDATED : OTA_SERVICE_ERROR;
}

static int OTAService_SaveReadyInfo(const OTA_TaskInfo_t *task)
{
  OTA_Info_t info;

  memset(&info, 0, sizeof(info));
  info.firmware_size = task->size;
  (void)snprintf(info.target_version, sizeof(info.target_version), "%s", task->target);
  (void)snprintf(info.task_id, sizeof(info.task_id), "%s", task->tid);
  (void)snprintf(info.expected_md5, sizeof(info.expected_md5), "%s", task->md5);

  if (OTAInfo_SaveReady(&info) != OTA_INFO_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  return OTA_SERVICE_UPDATED;
}

static int OTAService_SaveSimulateInfo(const OTA_TaskInfo_t *task)
{
  OTA_Info_t info;

  memset(&info, 0, sizeof(info));
  info.firmware_size = task->size;
  (void)snprintf(info.target_version, sizeof(info.target_version), "%s", task->target);
  (void)snprintf(info.task_id, sizeof(info.task_id), "%s", task->tid);
  (void)snprintf(info.expected_md5, sizeof(info.expected_md5), "%s", task->md5);

  if (OTAInfo_SaveSimulate(&info) != OTA_INFO_OK)
  {
    return OTA_SERVICE_ERROR;
  }

  return OTA_SERVICE_UPDATED;
}

int OTAService_CheckOnce(void)
{
  OTA_TaskInfo_t task;
  char current_version[OTA_TARGET_VERSION_LEN];
  char actual_md5[33];
  int result;

  OTAService_SyncRunningVersion();
  OTAService_GetReportVersion(current_version, (uint32_t)sizeof(current_version));

  Debug_Printf("[OTA] check start host=%s port=%u\r\n",
               OTA_HTTP_HOST,
               (unsigned int)OTA_HTTP_PORT);

  if (ESP8266_ConnectTcp(OTA_HTTP_HOST, OTA_HTTP_PORT) == 0U)
  {
    Debug_Printf("[OTA] tcp connect failed code=%u\r\n",
                 (unsigned int)ESP8266_GetLastInitStatus());
    return OTA_SERVICE_ERROR;
  }

  result = OTAService_PostPendingSimulateVersion();
  if (result == OTA_SERVICE_UPDATED)
  {
    ESP8266_CloseTcp();
    return OTA_SERVICE_NO_UPDATE;
  }
  if (result == OTA_SERVICE_ERROR)
  {
    ESP8266_CloseTcp();
    return OTA_SERVICE_ERROR;
  }

  result = OTAService_PostVersion(current_version);
  if (result == OTA_SERVICE_NO_UPDATE)
  {
    result = OTAService_QueryTask(&task, current_version);
  }

  if (result == OTA_SERVICE_UPDATED)
  {
    if (OTA_SIMULATE_UPGRADE_ONLY != 0U)
    {
      if (OTAService_SaveSimulateInfo(&task) == OTA_SERVICE_UPDATED)
      {
        Debug_Printf("[OTA] simulate ready set, reset to bootloader target=%s\r\n", task.target);
        ESP8266_CloseTcp();
        osDelay(200);
        NVIC_SystemReset();
      }
      result = OTA_SERVICE_ERROR;
    }
    else
    {
      memset(actual_md5, 0, sizeof(actual_md5));
      result = OTAService_DownloadFirmware(&task, actual_md5);
      if ((result == OTA_SERVICE_UPDATED) && (strcmp(actual_md5, task.md5) != 0))
      {
        Debug_Printf("[OTA] md5 mismatch\r\n");
        result = OTA_SERVICE_ERROR;
      }
      if (result == OTA_SERVICE_UPDATED)
      {
        if (OTAService_SaveReadyInfo(&task) == OTA_SERVICE_UPDATED)
        {
          Debug_Printf("[OTA] ready set, reset to bootloader\r\n");
          ESP8266_CloseTcp();
          osDelay(200);
          NVIC_SystemReset();
        }
        result = OTA_SERVICE_ERROR;
      }
    }
  }

  ESP8266_CloseTcp();
  return result;
}
