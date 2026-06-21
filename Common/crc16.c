#include "crc16.h"

#define CRC16_CCITT_POLY  0x1021U
#define CRC16_INIT_VALUE  0xFFFFU

uint16_t CRC16_Init(void)
{
  return CRC16_INIT_VALUE;
}

uint16_t CRC16_Update(uint16_t crc, const uint8_t *data, uint32_t length)
{
  uint32_t i;
  uint8_t bit;

  if ((data == 0) || (length == 0UL))
  {
    return crc;
  }

  for (i = 0UL; i < length; i++)
  {
    crc ^= (uint16_t)((uint16_t)data[i] << 8);

    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x8000U) != 0U)
      {
        crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
      }
      else
      {
        crc = (uint16_t)(crc << 1);
      }
    }
  }

  return crc;
}

uint16_t CRC16_Calculate(const uint8_t *data, uint32_t length)
{
  return CRC16_Update(CRC16_Init(), data, length);
}
