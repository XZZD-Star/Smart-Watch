#include "md5.h"

#include <string.h>

#define MD5_F(x, y, z) (((x) & (y)) | ((~(x)) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~(z))))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | (~(z))))
#define MD5_ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32U - (n))))
#define MD5_STEP(f, a, b, c, d, x, s, ac) \
  do { \
    (a) += f((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = MD5_ROTATE_LEFT((a), (s)); \
    (a) += (b); \
  } while (0)

static const uint8_t g_md5_padding[64] = {0x80U};

static uint32_t MD5_DecodeWord(const uint8_t *input)
{
  return ((uint32_t)input[0]) |
         ((uint32_t)input[1] << 8) |
         ((uint32_t)input[2] << 16) |
         ((uint32_t)input[3] << 24);
}

static void MD5_EncodeWord(uint32_t value, uint8_t *output)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}

static void MD5_Transform(uint32_t state[4], const uint8_t block[64])
{
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t x[16];
  uint8_t i;

  for (i = 0U; i < 16U; i++)
  {
    x[i] = MD5_DecodeWord(&block[(uint32_t)i * 4UL]);
  }

  MD5_STEP(MD5_F, a, b, c, d, x[ 0],  7, 0xd76aa478UL);
  MD5_STEP(MD5_F, d, a, b, c, x[ 1], 12, 0xe8c7b756UL);
  MD5_STEP(MD5_F, c, d, a, b, x[ 2], 17, 0x242070dbUL);
  MD5_STEP(MD5_F, b, c, d, a, x[ 3], 22, 0xc1bdceeeUL);
  MD5_STEP(MD5_F, a, b, c, d, x[ 4],  7, 0xf57c0fafUL);
  MD5_STEP(MD5_F, d, a, b, c, x[ 5], 12, 0x4787c62aUL);
  MD5_STEP(MD5_F, c, d, a, b, x[ 6], 17, 0xa8304613UL);
  MD5_STEP(MD5_F, b, c, d, a, x[ 7], 22, 0xfd469501UL);
  MD5_STEP(MD5_F, a, b, c, d, x[ 8],  7, 0x698098d8UL);
  MD5_STEP(MD5_F, d, a, b, c, x[ 9], 12, 0x8b44f7afUL);
  MD5_STEP(MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1UL);
  MD5_STEP(MD5_F, b, c, d, a, x[11], 22, 0x895cd7beUL);
  MD5_STEP(MD5_F, a, b, c, d, x[12],  7, 0x6b901122UL);
  MD5_STEP(MD5_F, d, a, b, c, x[13], 12, 0xfd987193UL);
  MD5_STEP(MD5_F, c, d, a, b, x[14], 17, 0xa679438eUL);
  MD5_STEP(MD5_F, b, c, d, a, x[15], 22, 0x49b40821UL);

  MD5_STEP(MD5_G, a, b, c, d, x[ 1],  5, 0xf61e2562UL);
  MD5_STEP(MD5_G, d, a, b, c, x[ 6],  9, 0xc040b340UL);
  MD5_STEP(MD5_G, c, d, a, b, x[11], 14, 0x265e5a51UL);
  MD5_STEP(MD5_G, b, c, d, a, x[ 0], 20, 0xe9b6c7aaUL);
  MD5_STEP(MD5_G, a, b, c, d, x[ 5],  5, 0xd62f105dUL);
  MD5_STEP(MD5_G, d, a, b, c, x[10],  9, 0x02441453UL);
  MD5_STEP(MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681UL);
  MD5_STEP(MD5_G, b, c, d, a, x[ 4], 20, 0xe7d3fbc8UL);
  MD5_STEP(MD5_G, a, b, c, d, x[ 9],  5, 0x21e1cde6UL);
  MD5_STEP(MD5_G, d, a, b, c, x[14],  9, 0xc33707d6UL);
  MD5_STEP(MD5_G, c, d, a, b, x[ 3], 14, 0xf4d50d87UL);
  MD5_STEP(MD5_G, b, c, d, a, x[ 8], 20, 0x455a14edUL);
  MD5_STEP(MD5_G, a, b, c, d, x[13],  5, 0xa9e3e905UL);
  MD5_STEP(MD5_G, d, a, b, c, x[ 2],  9, 0xfcefa3f8UL);
  MD5_STEP(MD5_G, c, d, a, b, x[ 7], 14, 0x676f02d9UL);
  MD5_STEP(MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8aUL);

  MD5_STEP(MD5_H, a, b, c, d, x[ 5],  4, 0xfffa3942UL);
  MD5_STEP(MD5_H, d, a, b, c, x[ 8], 11, 0x8771f681UL);
  MD5_STEP(MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122UL);
  MD5_STEP(MD5_H, b, c, d, a, x[14], 23, 0xfde5380cUL);
  MD5_STEP(MD5_H, a, b, c, d, x[ 1],  4, 0xa4beea44UL);
  MD5_STEP(MD5_H, d, a, b, c, x[ 4], 11, 0x4bdecfa9UL);
  MD5_STEP(MD5_H, c, d, a, b, x[ 7], 16, 0xf6bb4b60UL);
  MD5_STEP(MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70UL);
  MD5_STEP(MD5_H, a, b, c, d, x[13],  4, 0x289b7ec6UL);
  MD5_STEP(MD5_H, d, a, b, c, x[ 0], 11, 0xeaa127faUL);
  MD5_STEP(MD5_H, c, d, a, b, x[ 3], 16, 0xd4ef3085UL);
  MD5_STEP(MD5_H, b, c, d, a, x[ 6], 23, 0x04881d05UL);
  MD5_STEP(MD5_H, a, b, c, d, x[ 9],  4, 0xd9d4d039UL);
  MD5_STEP(MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5UL);
  MD5_STEP(MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8UL);
  MD5_STEP(MD5_H, b, c, d, a, x[ 2], 23, 0xc4ac5665UL);

  MD5_STEP(MD5_I, a, b, c, d, x[ 0],  6, 0xf4292244UL);
  MD5_STEP(MD5_I, d, a, b, c, x[ 7], 10, 0x432aff97UL);
  MD5_STEP(MD5_I, c, d, a, b, x[14], 15, 0xab9423a7UL);
  MD5_STEP(MD5_I, b, c, d, a, x[ 5], 21, 0xfc93a039UL);
  MD5_STEP(MD5_I, a, b, c, d, x[12],  6, 0x655b59c3UL);
  MD5_STEP(MD5_I, d, a, b, c, x[ 3], 10, 0x8f0ccc92UL);
  MD5_STEP(MD5_I, c, d, a, b, x[10], 15, 0xffeff47dUL);
  MD5_STEP(MD5_I, b, c, d, a, x[ 1], 21, 0x85845dd1UL);
  MD5_STEP(MD5_I, a, b, c, d, x[ 8],  6, 0x6fa87e4fUL);
  MD5_STEP(MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0UL);
  MD5_STEP(MD5_I, c, d, a, b, x[ 6], 15, 0xa3014314UL);
  MD5_STEP(MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1UL);
  MD5_STEP(MD5_I, a, b, c, d, x[ 4],  6, 0xf7537e82UL);
  MD5_STEP(MD5_I, d, a, b, c, x[11], 10, 0xbd3af235UL);
  MD5_STEP(MD5_I, c, d, a, b, x[ 2], 15, 0x2ad7d2bbUL);
  MD5_STEP(MD5_I, b, c, d, a, x[ 9], 21, 0xeb86d391UL);

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

void MD5_Init(MD5_Context_t *ctx)
{
  if (ctx == 0)
  {
    return;
  }

  ctx->count[0] = 0UL;
  ctx->count[1] = 0UL;
  ctx->state[0] = 0x67452301UL;
  ctx->state[1] = 0xefcdab89UL;
  ctx->state[2] = 0x98badcfeUL;
  ctx->state[3] = 0x10325476UL;
  memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void MD5_Update(MD5_Context_t *ctx, const uint8_t *data, uint32_t length)
{
  uint32_t index;
  uint32_t part_len;
  uint32_t i = 0UL;

  if ((ctx == 0) || (data == 0) || (length == 0UL))
  {
    return;
  }

  index = (ctx->count[0] >> 3) & 0x3FUL;
  ctx->count[0] += (length << 3);
  if (ctx->count[0] < (length << 3))
  {
    ctx->count[1]++;
  }
  ctx->count[1] += (length >> 29);

  part_len = 64UL - index;
  if (length >= part_len)
  {
    memcpy(&ctx->buffer[index], data, part_len);
    MD5_Transform(ctx->state, ctx->buffer);

    for (i = part_len; (i + 63UL) < length; i += 64UL)
    {
      MD5_Transform(ctx->state, &data[i]);
    }
    index = 0UL;
  }

  memcpy(&ctx->buffer[index], &data[i], length - i);
}

void MD5_Final(MD5_Context_t *ctx, uint8_t digest[16])
{
  uint8_t bits[8];
  uint32_t index;
  uint32_t pad_len;
  uint8_t i;

  if ((ctx == 0) || (digest == 0))
  {
    return;
  }

  MD5_EncodeWord(ctx->count[0], &bits[0]);
  MD5_EncodeWord(ctx->count[1], &bits[4]);

  index = (ctx->count[0] >> 3) & 0x3FUL;
  pad_len = (index < 56UL) ? (56UL - index) : (120UL - index);
  MD5_Update(ctx, g_md5_padding, pad_len);
  MD5_Update(ctx, bits, 8UL);

  for (i = 0U; i < 4U; i++)
  {
    MD5_EncodeWord(ctx->state[i], &digest[(uint32_t)i * 4UL]);
  }

  memset(ctx, 0, sizeof(*ctx));
}

void MD5_ToHex(const uint8_t digest[16], char out_hex[33])
{
  static const char hex[] = "0123456789abcdef";
  uint8_t i;

  if ((digest == 0) || (out_hex == 0))
  {
    return;
  }

  for (i = 0U; i < 16U; i++)
  {
    out_hex[(uint32_t)i * 2UL] = hex[digest[i] >> 4];
    out_hex[(uint32_t)i * 2UL + 1UL] = hex[digest[i] & 0x0FU];
  }
  out_hex[32] = '\0';
}

uint8_t MD5_IsHexString(const char *text)
{
  uint8_t i;

  if (text == 0)
  {
    return 0U;
  }

  for (i = 0U; i < 32U; i++)
  {
    char c = text[i];
    if (!(((c >= '0') && (c <= '9')) ||
          ((c >= 'a') && (c <= 'f')) ||
          ((c >= 'A') && (c <= 'F'))))
    {
      return 0U;
    }
  }

  return (text[32] == '\0') ? 1U : 0U;
}
