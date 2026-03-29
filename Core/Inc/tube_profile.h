#ifndef __TUBE_PROFILE_H
#define __TUBE_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "sdp810_driver.h"

#define TUBE_SENSOR_COUNT    (4U)

typedef enum
{
  TUBE_SENSOR_ROLE_DISABLED = 0U,
  TUBE_SENSOR_ROLE_PITOT = 1U,
  TUBE_SENSOR_ROLE_RADIAL = 2U,
  TUBE_SENSOR_ROLE_STATIC = 3U
} TubeSensorRole_t;

typedef struct
{
  const char *label;
  TubeSensorRole_t role;
  int16_t angle_deg;
  bool inverted;
  bool enabled;
} TubeSensorConfig_t;

typedef struct
{
  const char *name;
  const char *description;
  TubeSensorConfig_t sensors[TUBE_SENSOR_COUNT];
} TubeProfile_t;

typedef struct
{
  int32_t sensor_pressure_milli_pa[TUBE_SENSOR_COUNT];
  int32_t sensor_temperature_milli_c[TUBE_SENSOR_COUNT];
  bool pitot_valid;
  bool radial_valid;
  bool static_valid;
  uint8_t pitot_sensor_count;
  uint8_t radial_sensor_count;
  uint8_t static_sensor_count;
  int32_t pitot_dynamic_pressure_milli_pa;
  uint32_t pitot_airspeed_mm_per_s;
  int32_t radial_mean_pressure_milli_pa;
  int32_t radial_spread_milli_pa;
  int32_t radial_vector_x_milli_pa;
  int32_t radial_vector_y_milli_pa;
  uint32_t radial_vector_magnitude_milli_pa;
  int32_t radial_flow_angle_mdeg;
  int32_t static_mean_pressure_milli_pa;
} TubeMetrics_t;

const TubeProfile_t *TubeProfile_GetActive(void);
const char *TubeProfile_GetRoleName(TubeSensorRole_t role);
void TubeProfile_ComputeMetrics(const TubeProfile_t *profile,
                                const Sdp810Measurement_t measurements[TUBE_SENSOR_COUNT],
                                TubeMetrics_t *metrics);

#ifdef __cplusplus
}
#endif

#endif /* __TUBE_PROFILE_H */
