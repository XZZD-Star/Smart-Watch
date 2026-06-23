#include "ring_buffer.h"

static uint16_t RingBuffer_NextIndex(const ring_buffer_t *rb, uint16_t index)
{
  index++;

  if (index >= rb->size)
  {
    index = 0U;
  }

  return index;
}

int RingBuffer_Init(ring_buffer_t *out_rb, uint8_t *buffer, uint16_t size)
{
  if ((out_rb == NULL) || (buffer == NULL) || (size == 0U))
  {
    return -1;
  }

  out_rb->buffer = buffer;
  out_rb->size = size;
  out_rb->head = 0U;
  out_rb->tail = 0U;
  out_rb->count = 0U;

  return 0;
}

void RingBuffer_Reset(ring_buffer_t *rb)
{
  if (rb == NULL)
  {
    return;
  }

  rb->head = 0U;
  rb->tail = 0U;
  rb->count = 0U;
}

int RingBuffer_Write(ring_buffer_t *rb, const uint8_t *data, uint16_t length)
{
  uint16_t written;

  if ((rb == NULL) || (rb->buffer == NULL) || (data == NULL))
  {
    return -1;
  }

  written = 0U;

  while ((written < length) && (rb->count < rb->size))
  {
    rb->buffer[rb->head] = data[written];
    rb->head = RingBuffer_NextIndex(rb, rb->head);
    rb->count++;
    written++;
  }

  return (int)written;
}

int RingBuffer_Read(ring_buffer_t *rb, uint8_t *out_data, uint16_t length)
{
  uint16_t read;

  if ((rb == NULL) || (rb->buffer == NULL) || (out_data == NULL))
  {
    return -1;
  }

  read = 0U;

  while ((read < length) && (rb->count > 0U))
  {
    out_data[read] = rb->buffer[rb->tail];
    rb->tail = RingBuffer_NextIndex(rb, rb->tail);
    rb->count--;
    read++;
  }

  return (int)read;
}

uint16_t RingBuffer_Available(const ring_buffer_t *rb)
{
  if ((rb == NULL) || (rb->buffer == NULL))
  {
    return 0U;
  }

  return rb->count;
}

uint16_t RingBuffer_Space(const ring_buffer_t *rb)
{
  if ((rb == NULL) || (rb->buffer == NULL))
  {
    return 0U;
  }

  return (uint16_t)(rb->size - rb->count);
}

bool RingBuffer_IsEmpty(const ring_buffer_t *rb)
{
  if ((rb == NULL) || (rb->buffer == NULL))
  {
    return true;
  }

  return (rb->count == 0U);
}

bool RingBuffer_IsFull(const ring_buffer_t *rb)
{
  if ((rb == NULL) || (rb->buffer == NULL))
  {
    return false;
  }

  return (rb->count >= rb->size);
}
