#ifndef SCREEN_TASK_H
#define SCREEN_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS defaultTask 入口调用：维护 UART7 屏幕首页和训练计划页刷新。 */
void ScreenTask_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_TASK_H */
