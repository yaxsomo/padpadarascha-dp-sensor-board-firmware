#ifndef __FLIGHT_LOGGER_H
#define __FLIGHT_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "sdp810_driver.h"
#include "tube_profile.h"

typedef enum
{
  FLIGHT_LOGGER_STATE_INIT = 0U,
  FLIGHT_LOGGER_STATE_READY,
  FLIGHT_LOGGER_STATE_ACQUIRE,
  FLIGHT_LOGGER_STATE_WARNING_STORAGE,
  FLIGHT_LOGGER_STATE_STOPPED,
  FLIGHT_LOGGER_STATE_ERROR
} FlightLoggerState_t;

typedef enum
{
  FLIGHT_LOGGER_STAGE_IDLE = 0U,
  FLIGHT_LOGGER_STAGE_INIT_STORAGE,
  FLIGHT_LOGGER_STAGE_INIT_SENSORS,
  FLIGHT_LOGGER_STAGE_READY_WAIT,
  FLIGHT_LOGGER_STAGE_UPDATE_USAGE,
  FLIGHT_LOGGER_STAGE_READ_SENSORS,
  FLIGHT_LOGGER_STAGE_ROTATE_FILE,
  FLIGHT_LOGGER_STAGE_WRITE_ROW,
  FLIGHT_LOGGER_STAGE_SYNC_STOP,
  FLIGHT_LOGGER_STAGE_FATAL_SENSOR_CHECK
} FlightLoggerDebugStage_t;

typedef enum
{
  FLIGHT_LOGGER_ERROR_NONE = 0U,
  FLIGHT_LOGGER_ERROR_STORAGE_INIT,
  FLIGHT_LOGGER_ERROR_SENSOR_INIT,
  FLIGHT_LOGGER_ERROR_STORAGE_USAGE,
  FLIGHT_LOGGER_ERROR_ROTATE,
  FLIGHT_LOGGER_ERROR_WRITE_ROW,
  FLIGHT_LOGGER_ERROR_FATAL_SENSOR_FAILURE
} FlightLoggerErrorReason_t;

typedef struct
{
  uint64_t timestamp_us;
  uint32_t sample_counter;
  uint16_t consecutive_failures[4];
  uint8_t storage_usage_percent;
  FlightLoggerState_t state;
  FlightLoggerDebugStage_t last_stage;
  FlightLoggerErrorReason_t error_reason;
  TubeMetrics_t metrics;
  Sdp810Measurement_t measurements[4];
} FlightLoggerDebugSnapshot_t;

extern volatile FlightLoggerDebugSnapshot_t g_flight_logger_debug;

bool FlightLogger_Init(void);
void FlightLogger_RunOnce(void);
bool FlightLogger_IsLogging(void);
FlightLoggerState_t FlightLogger_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLIGHT_LOGGER_H */
