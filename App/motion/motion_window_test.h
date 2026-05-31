#ifndef MOTION_WINDOW_TEST_H
#define MOTION_WINDOW_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 任务启动时调用：初始化模型窗口测试请求状态。 */
void MotionWindowTest_Init(void);
/* 清除模型窗口测试请求状态。 */
void MotionWindowTest_Reset(void);
/* UART4/云端 start 在窗口测试模式下调用：请求跑一次固定窗口。 */
void MotionWindowTest_RequestRun(void);
/* 运动任务调用：若有待执行请求则运行一次固定窗口测试。 */
uint8_t MotionWindowTest_RunPending(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_WINDOW_TEST_H */
