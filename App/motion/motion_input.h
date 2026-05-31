#ifndef MOTION_INPUT_H
#define MOTION_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* UART4/云端调用：请求开始一次运动识别或模型窗口测试。 */
void Motion_RequestStart(void);
/* UART4/云端调用：清除当前跌倒告警状态。 */
void Motion_RequestClear(void);
/* 运动任务调用：取走 start 请求并触发 AI/规则状态重置。 */
uint8_t Motion_TakeRestartRequest(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_INPUT_H */
