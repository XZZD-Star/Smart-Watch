#ifndef MD5_H
#define MD5_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t state[4];
  uint32_t count[2];
  uint8_t buffer[64];
} MD5_Context_t;

void MD5_Init(MD5_Context_t *ctx);
void MD5_Update(MD5_Context_t *ctx, const uint8_t *data, uint32_t length);
void MD5_Final(MD5_Context_t *ctx, uint8_t digest[16]);
void MD5_ToHex(const uint8_t digest[16], char out_hex[33]);
uint8_t MD5_IsHexString(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* MD5_H */
