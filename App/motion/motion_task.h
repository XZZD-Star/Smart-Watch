#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#include "motion_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS Task1 入口调用：运行运动融合、AI/规则识别和动作结果排队。 */
void MotionTask_Run(void);
/**
 * @brief  读取运动任务缓存的最新前臂有效心率/血氧
 * @param  out_bio    输出前臂心率/血氧快照
 * @return 1 表示有有效缓存，0 表示暂无有效数据
 */
uint8_t MotionTask_GetLatestForeBio(motion_bio_sample_t *out_bio);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_TASK_H */
