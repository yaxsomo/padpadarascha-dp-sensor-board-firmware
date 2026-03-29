#include "tube_profile.h"

#include <math.h>
#include <string.h>

#include "configuration.h"

#define TUBE_PROFILE_PI    (3.14159265358979323846)

static const TubeProfile_t kActiveProfile = {
  LOGGER_PROFILE_NAME,
  "Per-sensor tube role configuration for pitot, radial, and static probes.",
  {
    {LOGGER_SENSOR_1_LABEL, (TubeSensorRole_t)LOGGER_SENSOR_1_TUBE_ROLE, LOGGER_SENSOR_1_ANGLE_DEG, (LOGGER_SENSOR_1_INVERTED != 0U), (LOGGER_SENSOR_1_ENABLED != 0U)},
    {LOGGER_SENSOR_2_LABEL, (TubeSensorRole_t)LOGGER_SENSOR_2_TUBE_ROLE, LOGGER_SENSOR_2_ANGLE_DEG, (LOGGER_SENSOR_2_INVERTED != 0U), (LOGGER_SENSOR_2_ENABLED != 0U)},
    {LOGGER_SENSOR_3_LABEL, (TubeSensorRole_t)LOGGER_SENSOR_3_TUBE_ROLE, LOGGER_SENSOR_3_ANGLE_DEG, (LOGGER_SENSOR_3_INVERTED != 0U), (LOGGER_SENSOR_3_ENABLED != 0U)},
    {LOGGER_SENSOR_4_LABEL, (TubeSensorRole_t)LOGGER_SENSOR_4_TUBE_ROLE, LOGGER_SENSOR_4_ANGLE_DEG, (LOGGER_SENSOR_4_INVERTED != 0U), (LOGGER_SENSOR_4_ENABLED != 0U)}
  }
};

const TubeProfile_t *TubeProfile_GetActive(void)
{
  return &kActiveProfile;
}

const char *TubeProfile_GetRoleName(TubeSensorRole_t role)
{
  switch (role)
  {
    case TUBE_SENSOR_ROLE_PITOT:
      return "PITOT";

    case TUBE_SENSOR_ROLE_RADIAL:
      return "RADIAL";

    case TUBE_SENSOR_ROLE_STATIC:
      return "STATIC";

    case TUBE_SENSOR_ROLE_DISABLED:
    default:
      return "DISABLED";
  }
}

void TubeProfile_ComputeMetrics(const TubeProfile_t *profile,
                                const Sdp810Measurement_t measurements[TUBE_SENSOR_COUNT],
                                TubeMetrics_t *metrics)
{
  uint32_t index;
  int64_t pitot_sum = 0;
  int64_t radial_sum = 0;
  int64_t static_sum = 0;
  int32_t radial_min = 0;
  int32_t radial_max = 0;
  bool radial_extrema_valid = false;
  double radial_vector_x = 0.0;
  double radial_vector_y = 0.0;

  memset(metrics, 0, sizeof(*metrics));

  for (index = 0U; index < TUBE_SENSOR_COUNT; ++index)
  {
    const TubeSensorConfig_t *sensor = &profile->sensors[index];
    int32_t pressure_milli_pa;
    int32_t temperature_milli_c;

    if ((sensor->enabled == false) ||
        (measurements[index].valid == false) ||
        (measurements[index].scale_factor == 0))
    {
      continue;
    }

    pressure_milli_pa = ((int32_t)measurements[index].differential_pressure_ticks * 1000L) /
                        (int32_t)measurements[index].scale_factor;
    if (sensor->inverted != false)
    {
      pressure_milli_pa = -pressure_milli_pa;
    }

    temperature_milli_c = (int32_t)measurements[index].temperature_ticks * 5L;

    metrics->sensor_pressure_milli_pa[index] = pressure_milli_pa;
    metrics->sensor_temperature_milli_c[index] = temperature_milli_c;

    switch (sensor->role)
    {
      case TUBE_SENSOR_ROLE_PITOT:
        metrics->pitot_sensor_count++;
        pitot_sum += pressure_milli_pa;
        break;

      case TUBE_SENSOR_ROLE_RADIAL:
      {
        double angle_rad = ((double)sensor->angle_deg * TUBE_PROFILE_PI) / 180.0;

        metrics->radial_sensor_count++;
        radial_sum += pressure_milli_pa;

        if (radial_extrema_valid == false)
        {
          radial_min = pressure_milli_pa;
          radial_max = pressure_milli_pa;
          radial_extrema_valid = true;
        }
        else
        {
          if (pressure_milli_pa < radial_min)
          {
            radial_min = pressure_milli_pa;
          }
          if (pressure_milli_pa > radial_max)
          {
            radial_max = pressure_milli_pa;
          }
        }

        radial_vector_x += (double)pressure_milli_pa * cos(angle_rad);
        radial_vector_y += (double)pressure_milli_pa * sin(angle_rad);
        break;
      }

      case TUBE_SENSOR_ROLE_STATIC:
        metrics->static_sensor_count++;
        static_sum += pressure_milli_pa;
        break;

      case TUBE_SENSOR_ROLE_DISABLED:
      default:
        break;
    }
  }

  if (metrics->pitot_sensor_count > 0U)
  {
    double airspeed_mm_per_s;
    double dynamic_pressure_pa;

    metrics->pitot_valid = true;
    metrics->pitot_dynamic_pressure_milli_pa = (int32_t)(pitot_sum / (int64_t)metrics->pitot_sensor_count);

    dynamic_pressure_pa = (double)metrics->pitot_dynamic_pressure_milli_pa / 1000.0;
    if ((dynamic_pressure_pa > 0.0) && (LOGGER_AIR_DENSITY_MG_PER_M3 != 0U))
    {
      double air_density_kg_per_m3 = (double)LOGGER_AIR_DENSITY_MG_PER_M3 / 1000000.0;

      airspeed_mm_per_s = sqrt((2.0 * dynamic_pressure_pa) / air_density_kg_per_m3) * 1000.0;
      if (airspeed_mm_per_s > 0.0)
      {
        metrics->pitot_airspeed_mm_per_s = (uint32_t)(airspeed_mm_per_s + 0.5);
      }
    }
  }

  if (metrics->radial_sensor_count > 0U)
  {
    double magnitude = sqrt((radial_vector_x * radial_vector_x) + (radial_vector_y * radial_vector_y));
    double angle_deg = atan2(radial_vector_y, radial_vector_x) * (180.0 / TUBE_PROFILE_PI);

    metrics->radial_valid = true;
    metrics->radial_mean_pressure_milli_pa = (int32_t)(radial_sum / (int64_t)metrics->radial_sensor_count);
    metrics->radial_spread_milli_pa = radial_max - radial_min;
    metrics->radial_vector_x_milli_pa = (int32_t)((radial_vector_x >= 0.0) ? (radial_vector_x + 0.5) : (radial_vector_x - 0.5));
    metrics->radial_vector_y_milli_pa = (int32_t)((radial_vector_y >= 0.0) ? (radial_vector_y + 0.5) : (radial_vector_y - 0.5));
    metrics->radial_vector_magnitude_milli_pa = (uint32_t)(magnitude + 0.5);
    metrics->radial_flow_angle_mdeg = (int32_t)((angle_deg >= 0.0) ? (angle_deg * 1000.0 + 0.5) : (angle_deg * 1000.0 - 0.5));
  }

  if (metrics->static_sensor_count > 0U)
  {
    metrics->static_valid = true;
    metrics->static_mean_pressure_milli_pa = (int32_t)(static_sum / (int64_t)metrics->static_sensor_count);
  }
}
