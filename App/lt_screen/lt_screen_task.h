#ifndef LT_SCREEN_TASK_H
#define LT_SCREEN_TASK_H

#include "lt168b.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTSCREEN_MODE_NORMAL    0U
#define LTSCREEN_MODE_NET_DEBUG 1U

#ifndef APP_LTSCREEN_MODE
#define APP_LTSCREEN_MODE LTSCREEN_MODE_NORMAL
#endif

#if ((APP_LTSCREEN_MODE != LTSCREEN_MODE_NORMAL) && \
     (APP_LTSCREEN_MODE != LTSCREEN_MODE_NET_DEBUG))
#error "APP_LTSCREEN_MODE must be LTSCREEN_MODE_NORMAL or LTSCREEN_MODE_NET_DEBUG"
#endif

void LTScreenTask_Run(void);
void LTScreen_HandleTouchEvent(const LT168B_TouchEvent_t *event);
/**
 * @brief  设置设备门状态
 * @param  is_open    0 表示关闭，非 0 表示打开
 */
void LTScreen_SetDeviceDoorState(uint8_t is_open);

#ifdef __cplusplus
}
#endif

#endif /* LT_SCREEN_TASK_H */
