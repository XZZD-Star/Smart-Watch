#ifndef __W25Q128_H
#define __W25Q128_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q128_OK              0
#define W25Q128_ERR_ID         -1
#define W25Q128_ERR_ERASE      -2
#define W25Q128_ERR_WRITE      -3
#define W25Q128_ERR_VERIFY     -4
#define W25Q128_ERR_TIMEOUT    -5

#define W25Q128_TEST_ADDR       0x210000UL

/* 第一版接线：CS=PG6，SCK=PB2，MOSI=PD11，MISO=PD12。 */
#define W25Q128_CS_GPIO_PORT    GPIOG
#define W25Q128_CS_GPIO_PIN     GPIO_PIN_6
#define W25Q128_SCK_GPIO_PORT   GPIOB
#define W25Q128_SCK_GPIO_PIN    GPIO_PIN_2
#define W25Q128_MOSI_GPIO_PORT  GPIOD
#define W25Q128_MOSI_GPIO_PIN   GPIO_PIN_11
#define W25Q128_MISO_GPIO_PORT  GPIOD
#define W25Q128_MISO_GPIO_PIN   GPIO_PIN_12

/**
 * @brief 初始化 W25Q128 软件 SPI GPIO。
 */
void W25Q128_Init(void);

/**
 * @brief 读取 JEDEC ID。
 */
void W25Q128_ReadID(uint8_t *out_mid, uint8_t *out_type, uint8_t *out_capacity);

/**
 * @brief 连续读取 10 次 JEDEC ID，用于检查 ID 是否稳定。
 */
void W25Q128_ReadIDLoopTest(void);

/**
 * @brief 擦除 4KB 扇区，address 建议 4KB 对齐。
 */
int W25Q128_SectorErase4KB(uint32_t address);

/**
 * @brief 页编程，length 最大 256 字节，且不能跨页。
 */
int W25Q128_PageProgram(uint32_t address, const uint8_t *data, uint16_t length);

/**
 * @brief 从指定地址读取数据。
 */
int W25Q128_ReadData(uint32_t address, uint8_t *data, uint32_t length);

/**
 * @brief 擦除覆盖 [address, address + length) 的所有 4KB 扇区。
 */
int W25Q128_EraseRange(uint32_t address, uint32_t length);

/**
 * @brief 按 256 字节页边界拆分写入数据，函数内部不自动擦除。
 */
int W25Q128_WriteData(uint32_t address, const uint8_t *data, uint32_t length);

/**
 * @brief Write data and read it back for byte-by-byte verification.
 */
int W25Q128_WriteDataVerified(uint32_t address, const uint8_t *data, uint32_t length);

/**
 * @brief 读取 ID、擦除、写入、读回并校验测试数据。
 */
int W25Q128_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q128_H */
