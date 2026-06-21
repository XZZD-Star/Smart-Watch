#ifndef BOOTLOADER_OTA_H
#define BOOTLOADER_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_OTA_NO_UPDATE   0
#define BOOT_OTA_INSTALLED   1
#define BOOT_OTA_ERROR      -1

int BootOTA_TryInstall(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_OTA_H */
