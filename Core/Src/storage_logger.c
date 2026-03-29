#include "storage_logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "configuration.h"
#include "fatfs.h"
#include "timebase_us.h"

static FIL s_log_file;
static char s_log_filename[14];
static char s_header_buffer[1024];

static bool StorageLogger_Mount(void);
static bool StorageLogger_OpenNextFile(StorageLogger_t *logger,
                                       uint64_t chunk_start_us,
                                       const TubeProfile_t *profile);
static bool StorageLogger_WriteHeader(StorageLogger_t *logger,
                                      const TubeProfile_t *profile,
                                      uint64_t chunk_start_us);
static bool StorageLogger_FlushAndClose(StorageLogger_t *logger);
static bool StorageLogger_FindNextIndex(uint16_t *next_index);
static bool StorageLogger_WriteText(const char *text);
static bool StorageLogger_WriteFormatted(const char *format, ...);
static void StorageLogger_FormatUint64(char *buffer, size_t buffer_size, uint64_t value);
static void StorageLogger_FormatFixedMilliSigned(char *buffer,
                                                 size_t buffer_size,
                                                 int32_t milli_value);
static void StorageLogger_FormatFixedMilliUnsigned(char *buffer,
                                                   size_t buffer_size,
                                                   uint32_t milli_value);
static void StorageLogger_FormatMetricOrNa(char *buffer,
                                           size_t buffer_size,
                                           bool valid,
                                           int32_t milli_value);
static void StorageLogger_FormatMetricUnsignedOrNa(char *buffer,
                                                   size_t buffer_size,
                                                   bool valid,
                                                   uint32_t milli_value);

bool StorageLogger_Init(StorageLogger_t *logger, const TubeProfile_t *profile)
{
  memset(logger, 0, sizeof(*logger));

  if (StorageLogger_Mount() == false)
  {
    printf("[STORAGE] Mount failed.\r\n");
    return false;
  }

  logger->mounted = true;

  if (StorageLogger_FindNextIndex(&logger->next_file_index) == false)
  {
    printf("[STORAGE] Failed to find next log index.\r\n");
    return false;
  }

  if (StorageLogger_UpdateUsage(logger) == false)
  {
    printf("[STORAGE] Failed to read initial usage.\r\n");
    return false;
  }

  printf("[STORAGE] Mounted. next_file=%u usage=%u%% profile=%s\r\n",
         (unsigned int)logger->next_file_index,
         (unsigned int)logger->usage_percent,
         profile->name);
  return StorageLogger_OpenNextFile(logger, Timebase_GetMicros(), profile);
}

void StorageLogger_Deinit(StorageLogger_t *logger)
{
  (void)StorageLogger_FlushAndClose(logger);

  if (logger->mounted != false)
  {
    (void)f_mount(NULL, SDPath, 1U);
    logger->mounted = false;
  }
}

bool StorageLogger_WriteRow(StorageLogger_t *logger,
                            uint64_t timestamp_us,
                            const TubeProfile_t *profile,
                            const Sdp810Measurement_t measurements[4],
                            const TubeMetrics_t *metrics)
{
  uint32_t index;
  char line_buffer[256];
  char timestamp_buffer[24];
  char pressure_buffer[20];
  char temperature_buffer[20];
  char metric_pitot_pressure[20];
  char metric_pitot_speed[20];
  char metric_radial_mean[20];
  char metric_radial_spread[20];
  char metric_radial_x[20];
  char metric_radial_y[20];
  char metric_radial_mag[20];
  char metric_radial_angle[20];
  char metric_static_mean[20];
  int length;

  if (logger->file_open == false)
  {
    return false;
  }

  StorageLogger_FormatUint64(timestamp_buffer, sizeof(timestamp_buffer), timestamp_us);
  length = snprintf(line_buffer, sizeof(line_buffer), "%s,%s",
                    timestamp_buffer, profile->name);
  if ((length < 0) || ((size_t)length >= sizeof(line_buffer)))
  {
    return false;
  }

  if (StorageLogger_WriteText(line_buffer) == false)
  {
    return false;
  }

  for (index = 0U; index < 4U; ++index)
  {
    if (measurements[index].valid != false)
    {
      StorageLogger_FormatFixedMilliSigned(pressure_buffer, sizeof(pressure_buffer),
                                           metrics->sensor_pressure_milli_pa[index]);
      StorageLogger_FormatFixedMilliSigned(temperature_buffer, sizeof(temperature_buffer),
                                           metrics->sensor_temperature_milli_c[index]);
    }
    else
    {
      (void)snprintf(pressure_buffer, sizeof(pressure_buffer), "NA");
      (void)snprintf(temperature_buffer, sizeof(temperature_buffer), "NA");
    }

    length = snprintf(line_buffer, sizeof(line_buffer), ",%s,%s,%s,%d,%d",
                      TubeProfile_GetRoleName(profile->sensors[index].role),
                      pressure_buffer,
                      temperature_buffer,
                      (int)measurements[index].scale_factor,
                      (int)measurements[index].status);
    if ((length < 0) || ((size_t)length >= sizeof(line_buffer)))
    {
      return false;
    }

    if (StorageLogger_WriteText(line_buffer) == false)
    {
      return false;
    }
  }

  StorageLogger_FormatMetricOrNa(metric_pitot_pressure, sizeof(metric_pitot_pressure),
                                 metrics->pitot_valid, metrics->pitot_dynamic_pressure_milli_pa);
  StorageLogger_FormatMetricUnsignedOrNa(metric_pitot_speed, sizeof(metric_pitot_speed),
                                         metrics->pitot_valid, metrics->pitot_airspeed_mm_per_s);
  StorageLogger_FormatMetricOrNa(metric_radial_mean, sizeof(metric_radial_mean),
                                 metrics->radial_valid, metrics->radial_mean_pressure_milli_pa);
  StorageLogger_FormatMetricOrNa(metric_radial_spread, sizeof(metric_radial_spread),
                                 metrics->radial_valid, metrics->radial_spread_milli_pa);
  StorageLogger_FormatMetricOrNa(metric_radial_x, sizeof(metric_radial_x),
                                 metrics->radial_valid, metrics->radial_vector_x_milli_pa);
  StorageLogger_FormatMetricOrNa(metric_radial_y, sizeof(metric_radial_y),
                                 metrics->radial_valid, metrics->radial_vector_y_milli_pa);
  StorageLogger_FormatMetricUnsignedOrNa(metric_radial_mag, sizeof(metric_radial_mag),
                                         metrics->radial_valid, metrics->radial_vector_magnitude_milli_pa);
  StorageLogger_FormatMetricOrNa(metric_radial_angle, sizeof(metric_radial_angle),
                                 metrics->radial_valid, metrics->radial_flow_angle_mdeg);
  StorageLogger_FormatMetricOrNa(metric_static_mean, sizeof(metric_static_mean),
                                 metrics->static_valid, metrics->static_mean_pressure_milli_pa);

  length = snprintf(line_buffer, sizeof(line_buffer), ",%s,%s,%s,%s,%s,%s,%s,%s,%s",
                    metric_pitot_pressure,
                    metric_pitot_speed,
                    metric_radial_mean,
                    metric_radial_spread,
                    metric_radial_x,
                    metric_radial_y,
                    metric_radial_mag,
                    metric_radial_angle,
                    metric_static_mean);
  if ((length < 0) || ((size_t)length >= sizeof(line_buffer)))
  {
    return false;
  }

  if (StorageLogger_WriteText(line_buffer) == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("\n") == false)
  {
    return false;
  }

  logger->rows_in_file++;
  logger->rows_since_sync++;

  if (logger->rows_since_sync >= LOGGER_FILE_SYNC_INTERVAL_ROWS)
  {
    if (f_sync(&s_log_file) != FR_OK)
    {
      return false;
    }
    logger->rows_since_sync = 0U;
  }

  return true;
}

bool StorageLogger_UpdateUsage(StorageLogger_t *logger)
{
  FATFS *fatfs = NULL;
  DWORD free_clusters = 0U;
  uint64_t total_clusters;
  uint64_t used_clusters;

  if (f_getfree(SDPath, &free_clusters, &fatfs) != FR_OK)
  {
    printf("[STORAGE] f_getfree failed.\r\n");
    return false;
  }

  if ((fatfs == NULL) || (fatfs->n_fatent <= 2U))
  {
    printf("[STORAGE] Invalid FATFS metadata.\r\n");
    return false;
  }

  total_clusters = (uint64_t)fatfs->n_fatent - 2ULL;
  used_clusters = total_clusters - (uint64_t)free_clusters;
  logger->usage_percent = (uint8_t)((used_clusters * 100ULL) / total_clusters);
  return true;
}

bool StorageLogger_ShouldRotate(const StorageLogger_t *logger, uint64_t timestamp_us)
{
  return (timestamp_us >= logger->next_rotation_us);
}

bool StorageLogger_Rotate(StorageLogger_t *logger, uint64_t timestamp_us, const TubeProfile_t *profile)
{
  if (StorageLogger_FlushAndClose(logger) == false)
  {
    return false;
  }

  return StorageLogger_OpenNextFile(logger, timestamp_us, profile);
}

bool StorageLogger_IsWarning(const StorageLogger_t *logger)
{
  return (logger->usage_percent >= LOGGER_STORAGE_WARNING_PERCENT);
}

bool StorageLogger_IsFull(const StorageLogger_t *logger)
{
  return (logger->usage_percent >= LOGGER_STORAGE_STOP_PERCENT);
}

uint8_t StorageLogger_GetUsagePercent(const StorageLogger_t *logger)
{
  return logger->usage_percent;
}

static bool StorageLogger_Mount(void)
{
  return (f_mount(&SDFatFS, SDPath, 1U) == FR_OK);
}

static bool StorageLogger_WriteText(const char *text)
{
  UINT bytes_to_write = (UINT)strlen(text);
  UINT bytes_written = 0U;

  if (f_write(&s_log_file, text, bytes_to_write, &bytes_written) != FR_OK)
  {
    return false;
  }

  return (bytes_written == bytes_to_write);
}

static bool StorageLogger_WriteFormatted(const char *format, ...)
{
  int length;
  va_list args;

  va_start(args, format);
  length = vsnprintf(s_header_buffer, sizeof(s_header_buffer), format, args);
  va_end(args);

  if ((length < 0) || ((size_t)length >= sizeof(s_header_buffer)))
  {
    return false;
  }

  return StorageLogger_WriteText(s_header_buffer);
}

static bool StorageLogger_OpenNextFile(StorageLogger_t *logger,
                                       uint64_t chunk_start_us,
                                       const TubeProfile_t *profile)
{
  FRESULT result;

  if (logger->next_file_index == 0U)
  {
    logger->next_file_index = 1U;
  }

  (void)snprintf(s_log_filename, sizeof(s_log_filename), "LOG_%04u.CSV", logger->next_file_index);
  result = f_open(&s_log_file, s_log_filename, FA_WRITE | FA_CREATE_NEW);
  if (result != FR_OK)
  {
    printf("[STORAGE] Failed to open %s (err=%d).\r\n", s_log_filename, (int)result);
    return false;
  }

  logger->file_open = true;
  logger->rows_in_file = 0U;
  logger->rows_since_sync = 0U;
  logger->current_chunk_start_us = chunk_start_us;
  logger->next_rotation_us = chunk_start_us + ((uint64_t)LOGGER_CHUNK_DURATION_SECONDS * 1000000ULL);
  logger->next_file_index++;

  printf("[STORAGE] Opened %s at t=%llu us.\r\n",
         s_log_filename,
         (unsigned long long)chunk_start_us);

  return StorageLogger_WriteHeader(logger, profile, chunk_start_us);
}

static bool StorageLogger_WriteHeader(StorageLogger_t *logger,
                                      const TubeProfile_t *profile,
                                      uint64_t chunk_start_us)
{
  char chunk_start_buffer[24];

  UNUSED(logger);

  if (StorageLogger_WriteFormatted("# Flight logger version,%s\n", LOGGER_VERSION_STRING) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Profile,%s\n", profile->name) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Sample rate Hz,%u\n", (unsigned int)LOGGER_SAMPLE_RATE_HZ) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Chunk duration s,%u\n", (unsigned int)LOGGER_CHUNK_DURATION_SECONDS) == false)
  {
    return false;
  }

  StorageLogger_FormatUint64(chunk_start_buffer, sizeof(chunk_start_buffer), chunk_start_us);
  if (StorageLogger_WriteFormatted("# Chunk start us,%s\n", chunk_start_buffer) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Sensor1,%s,%s,%d,%u,%u\n",
                                   profile->sensors[0].label,
                                   TubeProfile_GetRoleName(profile->sensors[0].role),
                                   (int)profile->sensors[0].angle_deg,
                                   (unsigned int)profile->sensors[0].inverted,
                                   (unsigned int)profile->sensors[0].enabled) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Sensor2,%s,%s,%d,%u,%u\n",
                                   profile->sensors[1].label,
                                   TubeProfile_GetRoleName(profile->sensors[1].role),
                                   (int)profile->sensors[1].angle_deg,
                                   (unsigned int)profile->sensors[1].inverted,
                                   (unsigned int)profile->sensors[1].enabled) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Sensor3,%s,%s,%d,%u,%u\n",
                                   profile->sensors[2].label,
                                   TubeProfile_GetRoleName(profile->sensors[2].role),
                                   (int)profile->sensors[2].angle_deg,
                                   (unsigned int)profile->sensors[2].inverted,
                                   (unsigned int)profile->sensors[2].enabled) == false)
  {
    return false;
  }

  if (StorageLogger_WriteFormatted("# Sensor4,%s,%s,%d,%u,%u\n",
                                   profile->sensors[3].label,
                                   TubeProfile_GetRoleName(profile->sensors[3].role),
                                   (int)profile->sensors[3].angle_deg,
                                   (unsigned int)profile->sensors[3].inverted,
                                   (unsigned int)profile->sensors[3].enabled) == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("timestamp_us,profile,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("sensor1_role,sensor1_pressure_pa,sensor1_temperature_c,sensor1_scale_factor,sensor1_status,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("sensor2_role,sensor2_pressure_pa,sensor2_temperature_c,sensor2_scale_factor,sensor2_status,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("sensor3_role,sensor3_pressure_pa,sensor3_temperature_c,sensor3_scale_factor,sensor3_status,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("sensor4_role,sensor4_pressure_pa,sensor4_temperature_c,sensor4_scale_factor,sensor4_status,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("pitot_dynamic_pressure_pa,pitot_airspeed_mps,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("radial_mean_pressure_pa,radial_spread_pa,radial_vector_x_pa,radial_vector_y_pa,radial_vector_magnitude_pa,radial_flow_angle_deg,") == false)
  {
    return false;
  }

  if (StorageLogger_WriteText("static_mean_pressure_pa\n") == false)
  {
    return false;
  }

  if (f_sync(&s_log_file) != FR_OK)
  {
    return false;
  }

  return true;
}

static void StorageLogger_FormatUint64(char *buffer, size_t buffer_size, uint64_t value)
{
  char temp[24];
  size_t count = 0U;
  size_t index;

  if (buffer_size == 0U)
  {
    return;
  }

  if (value == 0ULL)
  {
    if (buffer_size > 1U)
    {
      buffer[0] = '0';
      buffer[1] = '\0';
    }
    else
    {
      buffer[0] = '\0';
    }
    return;
  }

  while ((value > 0ULL) && (count < (sizeof(temp) - 1U)))
  {
    temp[count++] = (char)('0' + (value % 10ULL));
    value /= 10ULL;
  }

  if (count >= buffer_size)
  {
    buffer[0] = '\0';
    return;
  }

  for (index = 0U; index < count; ++index)
  {
    buffer[index] = temp[count - 1U - index];
  }
  buffer[count] = '\0';
}

static bool StorageLogger_FlushAndClose(StorageLogger_t *logger)
{
  FRESULT result;

  if (logger->file_open == false)
  {
    return true;
  }

  result = f_sync(&s_log_file);
  if (result != FR_OK)
  {
    return false;
  }

  result = f_close(&s_log_file);
  if (result != FR_OK)
  {
    return false;
  }

  logger->file_open = false;
  return true;
}

static bool StorageLogger_FindNextIndex(uint16_t *next_index)
{
  FILINFO file_info;
  char filename[14];
  uint16_t index;

  for (index = 1U; index < 10000U; ++index)
  {
    (void)snprintf(filename, sizeof(filename), "LOG_%04u.CSV", index);

    if (f_stat(filename, &file_info) == FR_NO_FILE)
    {
      *next_index = index;
      return true;
    }
  }

  return false;
}

static void StorageLogger_FormatFixedMilliSigned(char *buffer,
                                                 size_t buffer_size,
                                                 int32_t milli_value)
{
  int32_t absolute_value;
  int32_t whole;
  int32_t fractional;
  const char *sign = "";

  if (milli_value < 0)
  {
    sign = "-";
    absolute_value = -milli_value;
  }
  else
  {
    absolute_value = milli_value;
  }

  whole = absolute_value / 1000L;
  fractional = absolute_value % 1000L;

  (void)snprintf(buffer, buffer_size, "%s%ld.%03ld", sign, (long)whole, (long)fractional);
}

static void StorageLogger_FormatFixedMilliUnsigned(char *buffer,
                                                   size_t buffer_size,
                                                   uint32_t milli_value)
{
  uint32_t whole = milli_value / 1000UL;
  uint32_t fractional = milli_value % 1000UL;

  (void)snprintf(buffer, buffer_size, "%lu.%03lu",
                 (unsigned long)whole,
                 (unsigned long)fractional);
}

static void StorageLogger_FormatMetricOrNa(char *buffer,
                                           size_t buffer_size,
                                           bool valid,
                                           int32_t milli_value)
{
  if (valid == false)
  {
    (void)snprintf(buffer, buffer_size, "NA");
    return;
  }

  StorageLogger_FormatFixedMilliSigned(buffer, buffer_size, milli_value);
}

static void StorageLogger_FormatMetricUnsignedOrNa(char *buffer,
                                                   size_t buffer_size,
                                                   bool valid,
                                                   uint32_t milli_value)
{
  if (valid == false)
  {
    (void)snprintf(buffer, buffer_size, "NA");
    return;
  }

  StorageLogger_FormatFixedMilliUnsigned(buffer, buffer_size, milli_value);
}
