#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_FLASH_ADDR       0x08000000UL
#define BOOT_FLASH_SIZE       0x00020000UL

#define APP_FLASH_ADDR        0x08020000UL
#define APP_FLASH_SIZE        0x001E0000UL

#define INTERNAL_FLASH_END    0x08200000UL

#define W25Q128_TOTAL_SIZE    0x01000000UL

#define OTA_INFO_ADDR         0x000000UL
#define OTA_INFO_SIZE         0x00001000UL

#define OTA_RESERVED_ADDR     0x001000UL
#define OTA_RESERVED_SIZE     0x0000F000UL

#define OTA_BIN_ADDR          0x010000UL
#define OTA_BIN_REGION_SIZE   0x00200000UL
#define OTA_BIN_REGION_END    0x20FFFFUL

#define OTA_BIN_MAX_SIZE      APP_FLASH_SIZE

#define W25Q_TEST_ADDR        0x210000UL

#define OTA_INFO_MAGIC        0x4F544131UL
#define OTA_STATE_READY       0x5A5AA5A5UL

typedef struct
{
  uint32_t magic;
  uint32_t state;
  uint32_t firmware_size;
  uint16_t firmware_crc16;
  uint16_t reserved;
  uint32_t firmware_version;
} OTA_Info_t;

#ifdef __cplusplus
}
#endif

#endif /* OTA_LAYOUT_H */
