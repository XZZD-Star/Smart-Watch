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

#define OTA_DEVICE_INFO_A_ADDR 0x001000UL
#define OTA_DEVICE_INFO_B_ADDR 0x002000UL
#define OTA_DEVICE_INFO_SIZE   0x00001000UL

#define OTA_BIN_ADDR          0x010000UL
#define OTA_BIN_REGION_SIZE   0x00200000UL
#define OTA_BIN_REGION_END    0x20FFFFUL

#define OTA_BIN_MAX_SIZE      APP_FLASH_SIZE

#define W25Q_TEST_ADDR        0x210000UL

#define OTA_INFO_MAGIC        0x4F544132UL
#define OTA_DEVICE_INFO_MAGIC 0x44455632UL
#define OTA_READY_FLAG        1UL
#define OTA_SIMULATE_FLAG     2UL
#define OTA_INVALID_FLAG      0xFFFFFFFFUL

#define OTA_TARGET_VERSION_LEN  32U
#define OTA_TASK_ID_LEN         64U
#define OTA_MD5_HEX_LEN         32U

typedef struct
{
  uint32_t header_magic;
  uint32_t ready_flag;
  uint32_t firmware_size;
  char target_version[OTA_TARGET_VERSION_LEN];
  char task_id[OTA_TASK_ID_LEN];
  char expected_md5[OTA_MD5_HEX_LEN + 1U];
  uint8_t reserved;
  uint16_t info_crc16;
} OTA_Info_t;

typedef struct
{
  uint32_t header_magic;
  uint32_t sequence;
  char current_version[OTA_TARGET_VERSION_LEN];
  char previous_version[OTA_TARGET_VERSION_LEN];
  uint32_t last_upgrade_result;
  uint16_t info_crc16;
} OTA_DeviceInfo_t;

#ifdef __cplusplus
}
#endif

#endif /* OTA_LAYOUT_H */
