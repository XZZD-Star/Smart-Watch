#ifndef MOTION_SENSOR_PIPELINE_H
#define MOTION_SENSOR_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MOTION_SENSOR_ID_UPPER 0U
#define MOTION_SENSOR_ID_FORE  1U

extern volatile uint32_t g_lost_u;
extern volatile uint32_t g_lost_f;
extern volatile uint32_t g_align_fail_count;

extern volatile uint8_t  g_fused_row_ready;
extern volatile uint64_t g_fused_ts_us;
extern volatile float    g_fused_upper_yaw;
extern volatile float    g_fused_upper_pitch;
extern volatile float    g_fused_upper_roll;
extern volatile float    g_fused_fore_yaw;
extern volatile float    g_fused_fore_pitch;
extern volatile float    g_fused_fore_roll;
extern volatile uint32_t g_fused_seq_u;
extern volatile uint32_t g_fused_seq_f;
extern volatile uint32_t g_fused_lost_u;
extern volatile uint32_t g_fused_lost_f;
extern volatile int32_t  g_fused_upper_heart_rate;
extern volatile int32_t  g_fused_upper_spo2;
extern volatile int8_t   g_fused_upper_hr_valid;
extern volatile int8_t   g_fused_upper_spo2_valid;
extern volatile uint32_t g_fused_upper_ppg_fill;
extern volatile uint32_t g_fused_upper_ppg_calc_count;
extern volatile uint32_t g_fused_upper_ppg_pending;
extern volatile uint32_t g_fused_upper_ppg_part_id;
extern volatile uint32_t g_fused_upper_ppg_rev_id;
extern volatile uint32_t g_fused_upper_ppg_int_level;
extern volatile int32_t  g_fused_fore_heart_rate;
extern volatile int32_t  g_fused_fore_spo2;
extern volatile int8_t   g_fused_fore_hr_valid;
extern volatile int8_t   g_fused_fore_spo2_valid;
extern volatile uint32_t g_fused_fore_ppg_fill;
extern volatile uint32_t g_fused_fore_ppg_calc_count;
extern volatile uint32_t g_fused_fore_ppg_pending;
extern volatile uint32_t g_fused_fore_ppg_part_id;
extern volatile uint32_t g_fused_fore_ppg_rev_id;
extern volatile uint32_t g_fused_fore_ppg_int_level;

void MotionSensorPipeline_StorePacketFromIsr(uint8_t sensor_id, const uint8_t *buf, uint16_t len);
void MotionSensorPipeline_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_SENSOR_PIPELINE_H */
