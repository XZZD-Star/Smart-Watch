#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS Task1 入口调用：运行运动融合、AI/规则识别和动作结果排队。 */
void MotionTask_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_TASK_H */
