#ifndef __TIMEBASE_US_H
#define __TIMEBASE_US_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void Timebase_Init(void);
uint64_t Timebase_GetMicros(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIMEBASE_US_H */
