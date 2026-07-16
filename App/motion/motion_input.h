#ifndef MOTION_INPUT_H
#define MOTION_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* UART4/云端调用：请求开始一次运动识别或模型窗口测试。 */
void Motion_RequestStart(void);
/* UART4 ISR 调用：只投递开始请求，不在中断中执行管线复位。 */
void Motion_RequestStartFromIsr(void);
/* 屏幕/云端调用：请求停止当前运动识别会话。 */
void Motion_RequestStop(void);
/* UART4/云端调用：清除当前跌倒告警状态。 */
void Motion_RequestClear(void);
/* 运动任务调用：取走 start 请求并触发 AI/规则状态重置。 */
uint8_t Motion_TakeRestartRequest(void);
/* 运动任务调用：取走 UART4 ISR 投递的开始请求。 */
uint8_t Motion_TakeStartFromIsrRequest(void);
/* Task1 调用：取走 UART4 投递的单上臂采集复位请求。 */
uint8_t Motion_TakeUpperCaptureResetRequest(void);
/* Task1 调用：清空上臂接收链路并重新启动 USART1 DMA。 */
void Motion_ResetUpperCaptureInput(void);
/* 运动任务调用：取走 stop 请求并停止当前运动会话。 */
uint8_t Motion_TakeStopRequest(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_INPUT_H */
