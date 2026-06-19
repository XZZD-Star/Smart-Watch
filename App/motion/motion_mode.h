#ifndef MOTION_MODE_H
#define MOTION_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  MOTION_OUTPUT_MODE_RUN = 0,
  MOTION_OUTPUT_MODE_CAPTURE,
  MOTION_OUTPUT_MODE_BIO_CAPTURE,
  MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST,
  MOTION_OUTPUT_MODE_RULE_DEBUG
} motion_output_mode_t;

/* 默认输出模式：start 后进入正常识别输出。 */
//#define MOTION_OUTPUT_MODE_SELECT MOTION_OUTPUT_MODE_RUN
#define MOTION_OUTPUT_MODE_SELECT MOTION_OUTPUT_MODE_BIO_CAPTURE

/* 当前输出模式，由调试/编译配置切换，运动任务读取。 */
extern volatile motion_output_mode_t g_motion_output_mode;
/* 正常运行模式的启动闸门，由 start 命令置位。 */
extern volatile uint8_t g_motion_single_armed;

#ifdef __cplusplus
}
#endif

#endif /* MOTION_MODE_H */
