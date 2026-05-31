#ifndef MOTION_INPUT_H
#define MOTION_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t Motion_ProcessPendingPosePackets(void);
void Motion_RequestStart(void);
void Motion_RequestClear(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_INPUT_H */
