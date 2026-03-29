#ifndef __CONFIGURATION_H
#define __CONFIGURATION_H

#include <stdint.h>

typedef enum
{
  CONFIG_STATE_OFF = 0U,
  CONFIG_STATE_ON
} ConfigurationState_t;

typedef enum
{
  DEBUG_PRINT_INTERFACE_SWV = 0U,
  DEBUG_PRINT_INTERFACE_UART4
} DebugPrintInterface_t;

#define LOGGER_TUBE_ROLE_DISABLED                    (0U)
#define LOGGER_TUBE_ROLE_PITOT                       (1U)
#define LOGGER_TUBE_ROLE_RADIAL                      (2U)
#define LOGGER_TUBE_ROLE_STATIC                      (3U)

/*
 * Set to CONFIG_STATE_ON to force the buzzer off during board bring-up and LED
 * testing. Set to CONFIG_STATE_OFF to enable normal buzzer playback.
 */
#define BUZZER_OFF    (CONFIG_STATE_OFF)
#define DEBUG_PRINT_INTERFACE                    (DEBUG_PRINT_INTERFACE_SWV)

#define LOGGER_USB_BOOT_SETTLE_DELAY_MS            (3000U)
#define LOGGER_SAMPLE_RATE_HZ                      (100U)
#define LOGGER_CHUNK_DURATION_SECONDS              (300U)
#define LOGGER_STORAGE_WARNING_PERCENT             (85U)
#define LOGGER_STORAGE_STOP_PERCENT                (90U)
#define LOGGER_STORAGE_STATUS_INTERVAL_MS          (1000U)
#define LOGGER_FILE_SYNC_INTERVAL_ROWS             (50U)
#define LOGGER_SENSOR_READ_TIMEOUT_MS              (20U)
#define LOGGER_SENSOR_STARTUP_DELAY_MS             (10U)
#define LOGGER_SENSOR_CONSECUTIVE_WARNING_COUNT    (3U)
#define LOGGER_SENSOR_CONSECUTIVE_FATAL_COUNT      (25U)
#define LOGGER_DEBUG_SAMPLE_PRINT_INTERVAL         (100U)

#define LOGGER_SENSOR_1_I2C_ADDRESS                (0x25U)
#define LOGGER_SENSOR_2_I2C_ADDRESS                (0x25U)
#define LOGGER_SENSOR_3_I2C_ADDRESS                (0x25U)
#define LOGGER_SENSOR_4_I2C_ADDRESS                (0x25U)

#define LOGGER_PROFILE_NAME                        "RADIAL_RING_V1"
#define LOGGER_AIR_DENSITY_MG_PER_M3               (1225000U)

#define LOGGER_SENSOR_1_LABEL                      "S1_FRONT"
#define LOGGER_SENSOR_1_TUBE_ROLE                  (LOGGER_TUBE_ROLE_RADIAL)
#define LOGGER_SENSOR_1_ANGLE_DEG                  (0)
#define LOGGER_SENSOR_1_INVERTED                   (0U)
#define LOGGER_SENSOR_1_ENABLED                    (1U)

#define LOGGER_SENSOR_2_LABEL                      "S2_RIGHT"
#define LOGGER_SENSOR_2_TUBE_ROLE                  (LOGGER_TUBE_ROLE_RADIAL)
#define LOGGER_SENSOR_2_ANGLE_DEG                  (90)
#define LOGGER_SENSOR_2_INVERTED                   (0U)
#define LOGGER_SENSOR_2_ENABLED                    (1U)

#define LOGGER_SENSOR_3_LABEL                      "S3_REAR"
#define LOGGER_SENSOR_3_TUBE_ROLE                  (LOGGER_TUBE_ROLE_RADIAL)
#define LOGGER_SENSOR_3_ANGLE_DEG                  (180)
#define LOGGER_SENSOR_3_INVERTED                   (0U)
#define LOGGER_SENSOR_3_ENABLED                    (1U)

#define LOGGER_SENSOR_4_LABEL                      "S4_LEFT"
#define LOGGER_SENSOR_4_TUBE_ROLE                  (LOGGER_TUBE_ROLE_RADIAL)
#define LOGGER_SENSOR_4_ANGLE_DEG                  (270)
#define LOGGER_SENSOR_4_INVERTED                   (0U)
#define LOGGER_SENSOR_4_ENABLED                    (1U)

#define LOGGER_VERSION_STRING                      "1.0.0"

#endif /* __CONFIGURATION_H */
