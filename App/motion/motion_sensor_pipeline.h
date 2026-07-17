#ifndef MOTION_SENSOR_PIPELINE_H
#define MOTION_SENSOR_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "motion_frame.h"

#define MOTION_SENSOR_ID_UPPER 0U
#define MOTION_SENSOR_ID_FORE  1U

/* ISR 调用：保存一包原始姿态数据并通知运动任务，函数内部不做解析。 */
void MotionSensorPipeline_StorePacketFromIsr(uint8_t sensor_id, const uint8_t *buf, uint16_t len);
/* 运动任务调用：处理 ISR 暂存包，完成解析、丢包统计和双节点对齐。 */
uint8_t Motion_ProcessPendingPosePackets(void);
/* 运动任务调用：取走最近生成的一帧融合数据，成功返回 1。 */
uint8_t MotionSensorPipeline_TakeFusedFrame(motion_fused_frame_t *frame);
/* 配置单上臂输入；启用后，上臂包不等待前臂包即可生成输出帧。 */
void MotionSensorPipeline_SetUpperOnly(uint8_t enable);
void MotionSensorPipeline_RequestCalibration(void);
void MotionSensorPipeline_RequestUpperCalibration(void);
void MotionSensorPipeline_CalibrationTickFromIsr(void);
uint8_t MotionSensorPipeline_IsCalibrationActive(void);
uint8_t MotionSensorPipeline_IsUpperCalibrationActive(void);
uint8_t MotionSensorPipeline_IsCalibrationDone(void);
uint8_t MotionSensorPipeline_IsUpperCalibrationDone(void);
uint8_t MotionSensorPipeline_TakeCalibrationReport(float *out_fore_yaw,
                                                   float *out_fore_pitch,
                                                   float *out_fore_roll,
                                                   float *out_upper_yaw,
                                                   float *out_upper_pitch,
                                                   float *out_upper_roll);
uint8_t MotionSensorPipeline_TakeUpperCalibrationReport(float *out_yaw,
                                                        float *out_pitch,
                                                        float *out_roll);
/* 单上臂采集重启时调用：只清空上臂和待输出帧，不影响前臂状态。 */
void MotionSensorPipeline_ResetUpperCapture(void);
/* start/clear 场景调用：清空暂存包、序号统计和融合帧状态。 */
void MotionSensorPipeline_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_SENSOR_PIPELINE_H */
