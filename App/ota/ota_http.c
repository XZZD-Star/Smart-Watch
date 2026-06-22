#include "ota_http.h"

#include <stdio.h>
#include <string.h>

#include "ESP8266.h"

#define OTA_HTTP_RX_TIMEOUT_MS     8000U
#define OTA_HTTP_HEADER_MAX         768U
#define OTA_HTTP_IPD_MAX           1024U

static uint8_t g_ota_http_ipd_copy[OTA_HTTP_IPD_MAX];

static uint8_t OTAHttp_HeaderNameEquals(const char *line, const char *name, size_t name_len);

static void OTAHttp_ResetResponse(OTAHttp_Response_t *response)
{
  if (response != 0)
  {
    memset(response, 0, sizeof(*response));
  }
}

static char *OTAHttp_FindHeaderLine(char *headers, const char *name)
{
  char *line = headers;
  size_t name_len;

  if ((headers == 0) || (name == 0))
  {
    return 0;
  }

  name_len = strlen(name);
  while ((line != 0) && (*line != '\0'))
  {
    char *next = strstr(line, "\r\n");
    if ((OTAHttp_HeaderNameEquals(line, name, name_len) != 0U) && (line[name_len] == ':'))
    {
      return line + name_len + 1U;
    }

    if (next == 0)
    {
      break;
    }
    line = next + 2;
  }

  return 0;
}

static uint8_t OTAHttp_HeaderNameEquals(const char *line, const char *name, size_t name_len)
{
  size_t i;

  for (i = 0U; i < name_len; i++)
  {
    char a = line[i];
    char b = name[i];

    if ((a >= 'A') && (a <= 'Z'))
    {
      a = (char)(a - 'A' + 'a');
    }
    if ((b >= 'A') && (b <= 'Z'))
    {
      b = (char)(b - 'A' + 'a');
    }

    if (a != b)
    {
      return 0U;
    }
  }

  return 1U;
}

static char *OTAHttp_SkipSpaces(char *text)
{
  while ((text != 0) && ((*text == ' ') || (*text == '\t')))
  {
    text++;
  }

  return text;
}

static int OTAHttp_ParseHeader(char *headers, OTAHttp_Response_t *response)
{
  char *content_length;
  char *content_range;
  unsigned int status = 0U;
  unsigned long length = 0UL;
  unsigned long range_start = 0UL;
  unsigned long range_end = 0UL;
  unsigned long range_total = 0UL;

  if ((headers == 0) || (response == 0))
  {
    return OTA_HTTP_ERR_HEADER;
  }

  if (sscanf(headers, "HTTP/%*u.%*u %u", &status) != 1)
  {
    return OTA_HTTP_ERR_HEADER;
  }
  response->status_code = (uint16_t)status;

  content_length = OTAHttp_SkipSpaces(OTAHttp_FindHeaderLine(headers, "Content-Length"));
  if ((content_length == 0) || (sscanf(content_length, "%lu", &length) != 1))
  {
    return OTA_HTTP_ERR_LENGTH;
  }
  response->has_content_length = 1U;
  response->content_length = (uint32_t)length;

  content_range = OTAHttp_SkipSpaces(OTAHttp_FindHeaderLine(headers, "Content-Range"));
  if (content_range != 0)
  {
    if (sscanf(content_range, "bytes %lu-%lu/%lu", &range_start, &range_end, &range_total) != 3)
    {
      return OTA_HTTP_ERR_HEADER;
    }
    response->has_content_range = 1U;
    response->range_start = (uint32_t)range_start;
    response->range_end = (uint32_t)range_end;
    response->range_total = (uint32_t)range_total;
  }

  return OTA_HTTP_OK;
}

static int OTAHttp_DeliverBody(const uint8_t *data,
                               uint32_t length,
                               OTAHttp_Response_t *response,
                               OTAHttp_BodyCallback_t body_callback,
                               void *user)
{
  if (length == 0UL)
  {
    return OTA_HTTP_OK;
  }

  if ((response->body_received + length) > response->content_length)
  {
    return OTA_HTTP_ERR_LENGTH;
  }

  if ((body_callback != 0) && (body_callback(data, length, user) != OTA_HTTP_OK))
  {
    return OTA_HTTP_ERR;
  }

  response->body_received += length;
  return OTA_HTTP_OK;
}

int OTAHttp_SendRequest(const char *request,
                        uint32_t request_length,
                        OTAHttp_Response_t *response,
                        OTAHttp_BodyCallback_t body_callback,
                        void *user)
{
  char header[OTA_HTTP_HEADER_MAX];
  uint32_t header_len = 0UL;
  uint8_t header_done = 0U;
  int ret;

  if ((request == 0) || (request_length == 0UL) ||
      (request_length > 0xFFFFUL) || (response == 0))
  {
    return OTA_HTTP_ERR;
  }

  OTAHttp_ResetResponse(response);
  memset(header, 0, sizeof(header));

  if (ESP8266_SendData((const uint8_t *)request, (uint16_t)request_length) == 0U)
  {
    return OTA_HTTP_ERR;
  }

  while ((header_done == 0U) ||
         ((response->has_content_length != 0U) &&
          (response->body_received < response->content_length)))
  {
    uint8_t *ipd_payload = ESP8266_GetIPD(OTA_HTTP_RX_TIMEOUT_MS);
    uint32_t ipd_len = ESP8266_GetLastIPDLength();
    uint32_t offset = 0UL;

    if (ipd_payload == 0)
    {
      return OTA_HTTP_ERR_TIMEOUT;
    }

    if ((ipd_len == 0UL) || (ipd_len > OTA_HTTP_IPD_MAX))
    {
      ESP8266_Clear();
      return OTA_HTTP_ERR_LENGTH;
    }

    memcpy(g_ota_http_ipd_copy, ipd_payload, ipd_len);
    ESP8266_Clear();

    if (header_done == 0U)
    {
      while (offset < ipd_len)
      {
        if (header_len >= (OTA_HTTP_HEADER_MAX - 1UL))
        {
          return OTA_HTTP_ERR_HEADER;
        }

        header[header_len++] = (char)g_ota_http_ipd_copy[offset++];
        header[header_len] = '\0';

        if ((header_len >= 4UL) &&
            (memcmp(&header[header_len - 4UL], "\r\n\r\n", 4U) == 0))
        {
          header_done = 1U;
          ret = OTAHttp_ParseHeader(header, response);
          if (ret != OTA_HTTP_OK)
          {
            return ret;
          }
          break;
        }
      }
    }

    if (header_done != 0U)
    {
      ret = OTAHttp_DeliverBody(&g_ota_http_ipd_copy[offset],
                                ipd_len - offset,
                                response,
                                body_callback,
                                user);
      if (ret != OTA_HTTP_OK)
      {
        return ret;
      }
    }
  }

  if ((response->has_content_length == 0U) ||
      (response->body_received != response->content_length))
  {
    return OTA_HTTP_ERR_LENGTH;
  }

  return OTA_HTTP_OK;
}
