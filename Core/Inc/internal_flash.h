#ifndef INTERNAL_FLASH_H
#define INTERNAL_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTERNAL_FLASH_OK      0
#define INTERNAL_FLASH_ERR    -1

int InternalFlash_EraseAppArea(uint32_t app_start, uint32_t app_size);
int InternalFlash_Write(uint32_t address, const uint8_t *data, uint32_t length);
int InternalFlash_Verify(uint32_t address, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_FLASH_H */
