#include "flight_logger.h"

#include <stdio.h>
#include <string.h>

#include "configuration.h"
#include "feedback.h"
#include "sdp810_driver.h"
#include "storage_logger.h"
#include "timebase_us.h"
#include "tube_profile.h"

typedef struct
{
  uint8_t initialized;
  uint8_t warning_beep_latched;
  uint8_t storage_warning_latched;
  uint8_t fatal_error_latched;
  uint32_t next_feedback_tick_ms;
  uint32_t next_storage_tick_ms;
  uint32_t sample_counter;
  uint64_t ready_until_us;
  uint64_t next_sample_us;
  uint16_t consecutive_failures[4];
  FlightLoggerState_t state;
  StorageLogger_t storage;
  const TubeProfile_t *profile;
} FlightLoggerContext_t;

static FlightLoggerContext_t s_logger;
volatile FlightLoggerDebugSnapshot_t g_flight_logger_debug;

static void FlightLogger_SetState(FlightLoggerState_t state);
static void FlightLogger_UpdateFeedback(uint32_t now_ms);
static bool FlightLogger_ProcessSample(void);
static bool FlightLogger_HasFatalSensorFailure(void);

bool FlightLogger_Init(void)
{
  memset(&s_logger, 0, sizeof(s_logger));
  memset((void *)&g_flight_logger_debug, 0, sizeof(g_flight_logger_debug));

  Feedback_Init();
  Feedback_TriggerPattern(FEEDBACK_PATTERN_STARTUP);
  Timebase_Init();

  s_logger.profile = TubeProfile_GetActive();
  s_logger.state = FLIGHT_LOGGER_STATE_INIT;
  s_logger.next_storage_tick_ms = HAL_GetTick();
  g_flight_logger_debug.state = s_logger.state;
  g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_INIT_STORAGE;
  g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_NONE;
  printf("[LOGGER] Init start. profile=%s sample_rate=%uHz chunk=%us warn=%u%% stop=%u%%\r\n",
         s_logger.profile->name,
         (unsigned int)LOGGER_SAMPLE_RATE_HZ,
         (unsigned int)LOGGER_CHUNK_DURATION_SECONDS,
         (unsigned int)LOGGER_STORAGE_WARNING_PERCENT,
         (unsigned int)LOGGER_STORAGE_STOP_PERCENT);

  if (StorageLogger_Init(&s_logger.storage, s_logger.profile) == false)
  {
    g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_STORAGE_INIT;
    printf("[LOGGER] Storage initialization failed.\r\n");
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_ERROR);
    s_logger.initialized = 1U;
    return false;
  }

  g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_INIT_SENSORS;
  if (Sdp810Driver_Init() == false)
  {
    g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_SENSOR_INIT;
    printf("[LOGGER] Sensor initialization failed.\r\n");
    StorageLogger_Deinit(&s_logger.storage);
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_ERROR);
    s_logger.initialized = 1U;
    return false;
  }

  s_logger.ready_until_us = Timebase_GetMicros() + 500000ULL;
  s_logger.next_sample_us = s_logger.ready_until_us + (1000000ULL / LOGGER_SAMPLE_RATE_HZ);
  s_logger.initialized = 1U;

  FlightLogger_SetState(FLIGHT_LOGGER_STATE_READY);
  g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_READY_WAIT;
  printf("[LOGGER] Ready window active for 500 ms before acquisition.\r\n");
  return true;
}

void FlightLogger_RunOnce(void)
{
  uint64_t now_us;
  uint32_t now_ms;

  if (s_logger.initialized == 0U)
  {
    return;
  }

  now_us = Timebase_GetMicros();
  now_ms = HAL_GetTick();

  Feedback_Update(now_ms);
  FlightLogger_UpdateFeedback(now_ms);

  if ((now_ms - s_logger.next_storage_tick_ms) >= LOGGER_STORAGE_STATUS_INTERVAL_MS)
  {
    g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_UPDATE_USAGE;
    if (StorageLogger_UpdateUsage(&s_logger.storage) == false)
    {
      g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_STORAGE_USAGE;
      printf("[LOGGER] Failed to refresh storage usage.\r\n");
      FlightLogger_SetState(FLIGHT_LOGGER_STATE_ERROR);
      return;
    }

    printf("[LOGGER] Storage usage=%u%% state=%u\r\n",
           (unsigned int)StorageLogger_GetUsagePercent(&s_logger.storage),
           (unsigned int)s_logger.state);
    s_logger.next_storage_tick_ms = now_ms;
  }

  if (StorageLogger_IsFull(&s_logger.storage) != false)
  {
    g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_SYNC_STOP;
    printf("[LOGGER] Storage stop threshold reached (%u%%). Stopping acquisition.\r\n",
           (unsigned int)StorageLogger_GetUsagePercent(&s_logger.storage));
    Sdp810Driver_Stop();
    StorageLogger_Deinit(&s_logger.storage);
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_STOPPED);
    return;
  }

  if (s_logger.state == FLIGHT_LOGGER_STATE_READY)
  {
    if (now_us >= s_logger.ready_until_us)
    {
      FlightLogger_SetState(FLIGHT_LOGGER_STATE_ACQUIRE);
    }
    return;
  }

  if ((s_logger.state != FLIGHT_LOGGER_STATE_ACQUIRE) &&
      (s_logger.state != FLIGHT_LOGGER_STATE_WARNING_STORAGE))
  {
    return;
  }

  if (now_us < s_logger.next_sample_us)
  {
    return;
  }

  if (FlightLogger_ProcessSample() == false)
  {
    printf("[LOGGER] Sample processing failed.\r\n");
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_ERROR);
    return;
  }

  if (StorageLogger_IsWarning(&s_logger.storage) != false)
  {
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_WARNING_STORAGE);
  }
  else
  {
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_ACQUIRE);
  }

  if (FlightLogger_HasFatalSensorFailure() != false)
  {
    g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_FATAL_SENSOR_CHECK;
    g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_FATAL_SENSOR_FAILURE;
    printf("[LOGGER] Fatal sensor failure threshold reached. Stopping logger.\r\n");
    Sdp810Driver_Stop();
    StorageLogger_Deinit(&s_logger.storage);
    FlightLogger_SetState(FLIGHT_LOGGER_STATE_ERROR);
    return;
  }

  s_logger.next_sample_us += (1000000ULL / LOGGER_SAMPLE_RATE_HZ);
  if (now_us > (s_logger.next_sample_us + (1000000ULL / LOGGER_SAMPLE_RATE_HZ)))
  {
    s_logger.next_sample_us = now_us + (1000000ULL / LOGGER_SAMPLE_RATE_HZ);
  }
}

bool FlightLogger_IsLogging(void)
{
  return ((s_logger.state == FLIGHT_LOGGER_STATE_READY) ||
          (s_logger.state == FLIGHT_LOGGER_STATE_ACQUIRE) ||
          (s_logger.state == FLIGHT_LOGGER_STATE_WARNING_STORAGE));
}

FlightLoggerState_t FlightLogger_GetState(void)
{
  return s_logger.state;
}

static void FlightLogger_SetState(FlightLoggerState_t state)
{
  if (s_logger.state == state)
  {
    return;
  }

  s_logger.state = state;
  g_flight_logger_debug.state = state;

  switch (state)
  {
    case FLIGHT_LOGGER_STATE_READY:
      printf("[LOGGER] State -> READY\r\n");
      Feedback_SetColor(FEEDBACK_COLOR_BLUE);
      break;

    case FLIGHT_LOGGER_STATE_ACQUIRE:
      printf("[LOGGER] State -> ACQUIRE\r\n");
      Feedback_SetColor(FEEDBACK_COLOR_GREEN);
      s_logger.storage_warning_latched = 0U;
      break;

    case FLIGHT_LOGGER_STATE_WARNING_STORAGE:
      printf("[LOGGER] State -> WARNING_STORAGE\r\n");
      if (s_logger.storage_warning_latched == 0U)
      {
        Feedback_TriggerPattern(FEEDBACK_PATTERN_WARNING);
        s_logger.storage_warning_latched = 1U;
      }
      break;

    case FLIGHT_LOGGER_STATE_STOPPED:
      printf("[LOGGER] State -> STOPPED\r\n");
      Feedback_SetColor(FEEDBACK_COLOR_YELLOW);
      Feedback_PlaySound(FEEDBACK_SOUND_WARNING);
      break;

    case FLIGHT_LOGGER_STATE_ERROR:
      printf("[LOGGER] State -> ERROR\r\n");
      if (s_logger.fatal_error_latched == 0U)
      {
        Feedback_TriggerPattern(FEEDBACK_PATTERN_ERROR);
        s_logger.fatal_error_latched = 1U;
      }
      break;

    case FLIGHT_LOGGER_STATE_INIT:
    default:
      break;
  }
}

static void FlightLogger_UpdateFeedback(uint32_t now_ms)
{
  if ((int32_t)(now_ms - s_logger.next_feedback_tick_ms) < 0)
  {
    return;
  }

  s_logger.next_feedback_tick_ms = now_ms + 1000U;

  if (s_logger.state == FLIGHT_LOGGER_STATE_ACQUIRE)
  {
    Feedback_SetColor(FEEDBACK_COLOR_GREEN);
  }
  else if (s_logger.state == FLIGHT_LOGGER_STATE_WARNING_STORAGE)
  {
    Feedback_SetColor(FEEDBACK_COLOR_ORANGE);

    if (s_logger.warning_beep_latched == 0U)
    {
      Feedback_PlaySound(FEEDBACK_SOUND_WARNING);
      s_logger.warning_beep_latched = 1U;
    }
    else
    {
      s_logger.warning_beep_latched = 0U;
    }
  }
}

static bool FlightLogger_ProcessSample(void)
{
  Sdp810Measurement_t measurements[4];
  TubeMetrics_t metrics;
  uint64_t timestamp_us;
  uint32_t index;

  timestamp_us = Timebase_GetMicros();
  g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_READ_SENSORS;
  Sdp810Driver_ReadAll(measurements);
  TubeProfile_ComputeMetrics(s_logger.profile, measurements, &metrics);

  for (index = 0U; index < 4U; ++index)
  {
    if (measurements[index].valid != false)
    {
      s_logger.consecutive_failures[index] = 0U;
    }
    else
    {
      if (s_logger.consecutive_failures[index] < UINT16_MAX)
      {
        s_logger.consecutive_failures[index]++;
      }

      if (s_logger.consecutive_failures[index] >= LOGGER_SENSOR_CONSECUTIVE_WARNING_COUNT)
      {
        printf("[LOGGER] Sensor %lu warning failure_count=%u status=%d\r\n",
               (unsigned long)(index + 1U),
               (unsigned int)s_logger.consecutive_failures[index],
               (int)measurements[index].status);
        Feedback_SetColor(FEEDBACK_COLOR_MAGENTA);
        if (s_logger.consecutive_failures[index] == LOGGER_SENSOR_CONSECUTIVE_WARNING_COUNT)
        {
          Feedback_PlaySound(FEEDBACK_SOUND_NOTIFICATION);
        }
      }
    }

    g_flight_logger_debug.measurements[index] = measurements[index];
    g_flight_logger_debug.consecutive_failures[index] = s_logger.consecutive_failures[index];
  }

  g_flight_logger_debug.metrics = metrics;

  if (StorageLogger_ShouldRotate(&s_logger.storage, timestamp_us) != false)
  {
    g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_ROTATE_FILE;
    printf("[LOGGER] Rotating CSV chunk at t=%llu us\r\n", (unsigned long long)timestamp_us);
    if (StorageLogger_Rotate(&s_logger.storage, timestamp_us, s_logger.profile) == false)
    {
      g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_ROTATE;
      return false;
    }
  }

  g_flight_logger_debug.last_stage = FLIGHT_LOGGER_STAGE_WRITE_ROW;
  if (StorageLogger_WriteRow(&s_logger.storage, timestamp_us, s_logger.profile, measurements, &metrics) == false)
  {
    g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_WRITE_ROW;
    return false;
  }

  s_logger.sample_counter++;
  g_flight_logger_debug.timestamp_us = timestamp_us;
  g_flight_logger_debug.sample_counter = s_logger.sample_counter;
  g_flight_logger_debug.storage_usage_percent = StorageLogger_GetUsagePercent(&s_logger.storage);
  g_flight_logger_debug.state = s_logger.state;
  g_flight_logger_debug.error_reason = FLIGHT_LOGGER_ERROR_NONE;
  if ((LOGGER_DEBUG_SAMPLE_PRINT_INTERVAL != 0U) &&
      ((s_logger.sample_counter % LOGGER_DEBUG_SAMPLE_PRINT_INTERVAL) == 0U))
  {
    printf("[LOGGER] Sample %lu t=%llu us s1=%d s2=%d s3=%d s4=%d\r\n",
           (unsigned long)s_logger.sample_counter,
           (unsigned long long)timestamp_us,
           (int)metrics.sensor_pressure_milli_pa[0],
           (int)metrics.sensor_pressure_milli_pa[1],
           (int)metrics.sensor_pressure_milli_pa[2],
           (int)metrics.sensor_pressure_milli_pa[3]);
  }

  return true;
}

static bool FlightLogger_HasFatalSensorFailure(void)
{
  uint32_t index;

  for (index = 0U; index < 4U; ++index)
  {
    if (s_logger.consecutive_failures[index] >= LOGGER_SENSOR_CONSECUTIVE_FATAL_COUNT)
    {
      return true;
    }
  }

  return false;
}
