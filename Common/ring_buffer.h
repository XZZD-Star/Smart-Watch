#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint8_t *buffer;
  uint16_t size;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} ring_buffer_t;

/**
 * @brief  初始化环形缓冲区
 * @param  out_rb  环形缓冲区对象
 * @param  buffer  存储数据的外部缓冲区
 * @param  size    缓冲区大小
 * @return 0 成功，-1 失败
 */
int RingBuffer_Init(ring_buffer_t *out_rb, uint8_t *buffer, uint16_t size);

/**
 * @brief  清空环形缓冲区
 * @param  rb  环形缓冲区对象
 */
void RingBuffer_Reset(ring_buffer_t *rb);

/**
 * @brief  向环形缓冲区写入数据
 * @param  rb      环形缓冲区对象
 * @param  data    待写入数据
 * @param  length  写入长度
 * @return 实际写入字节数，-1 表示参数错误
 */
int RingBuffer_Write(ring_buffer_t *rb, const uint8_t *data, uint16_t length);

/**
 * @brief  从环形缓冲区读取数据
 * @param  rb         环形缓冲区对象
 * @param  out_data   输出缓冲区
 * @param  length     读取长度
 * @return 实际读取字节数，-1 表示参数错误
 */
int RingBuffer_Read(ring_buffer_t *rb, uint8_t *out_data, uint16_t length);

/**
 * @brief  获取当前可读数据长度
 * @param  rb  环形缓冲区对象
 * @return 当前可读字节数
 */
uint16_t RingBuffer_Available(const ring_buffer_t *rb);

/**
 * @brief  获取当前剩余空间
 * @param  rb  环形缓冲区对象
 * @return 当前剩余可写字节数
 */
uint16_t RingBuffer_Space(const ring_buffer_t *rb);

/**
 * @brief  判断环形缓冲区是否为空
 * @param  rb  环形缓冲区对象
 * @return true 表示为空，false 表示非空
 */
bool RingBuffer_IsEmpty(const ring_buffer_t *rb);

/**
 * @brief  判断环形缓冲区是否已满
 * @param  rb  环形缓冲区对象
 * @return true 表示已满，false 表示未满
 */
bool RingBuffer_IsFull(const ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
