#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t CRC16_Init(void);
uint16_t CRC16_Update(uint16_t crc, const uint8_t *data, uint32_t length);
uint16_t CRC16_Calculate(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
