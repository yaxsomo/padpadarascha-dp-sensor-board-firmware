#ifndef __DEBUG_OUTPUT_H
#define __DEBUG_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

void DebugOutput_Init(void);
int DebugOutput_Write(const char *data, int length);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_OUTPUT_H */
