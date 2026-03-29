#ifndef __STORAGE_LOGGER_H
#define __STORAGE_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "sdp810_driver.h"
#include "tube_profile.h"

typedef struct
{
  bool mounted;
  bool file_open;
  uint16_t next_file_index;
  uint16_t rows_in_file;
  uint16_t rows_since_sync;
  uint8_t usage_percent;
  uint64_t current_chunk_start_us;
  uint64_t next_rotation_us;
} StorageLogger_t;

bool StorageLogger_Init(StorageLogger_t *logger, const TubeProfile_t *profile);
void StorageLogger_Deinit(StorageLogger_t *logger);
bool StorageLogger_WriteRow(StorageLogger_t *logger,
                            uint64_t timestamp_us,
                            const TubeProfile_t *profile,
                            const Sdp810Measurement_t measurements[4],
                            const TubeMetrics_t *metrics);
bool StorageLogger_UpdateUsage(StorageLogger_t *logger);
bool StorageLogger_ShouldRotate(const StorageLogger_t *logger, uint64_t timestamp_us);
bool StorageLogger_Rotate(StorageLogger_t *logger, uint64_t timestamp_us, const TubeProfile_t *profile);
bool StorageLogger_IsWarning(const StorageLogger_t *logger);
bool StorageLogger_IsFull(const StorageLogger_t *logger);
uint8_t StorageLogger_GetUsagePercent(const StorageLogger_t *logger);

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_LOGGER_H */
