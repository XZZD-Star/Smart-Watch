#ifndef MOTION_FRAME_H
#define MOTION_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 单个传感器随姿态帧一起上报的生理数据快照。 */
typedef struct
{
  int32_t heart_rate;
  int32_t spo2;
  int8_t hr_valid;
  int8_t spo2_valid;
  uint32_t ppg_fill;
  uint32_t ppg_calc_count;
  uint32_t ppg_pending;
  uint32_t ppg_part_id;
  uint32_t ppg_rev_id;
  uint32_t ppg_int_level;
} motion_bio_sample_t;

/* 上臂和前臂按时间对齐后的融合帧，是运动识别链路的统一输入。 */
typedef struct
{
  uint64_t ts_us;
  float upper_yaw;
  float upper_pitch;
  float upper_roll;
  float fore_yaw;
  float fore_pitch;
  float fore_roll;
  uint32_t seq_u;
  uint32_t seq_f;
  uint32_t lost_u;
  uint32_t lost_f;
  motion_bio_sample_t upper_bio;
  motion_bio_sample_t fore_bio;
  uint32_t align_fail_count;
} motion_fused_frame_t;

#ifdef __cplusplus
}
#endif

#endif /* MOTION_FRAME_H */
