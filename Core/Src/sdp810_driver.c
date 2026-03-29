#include "sdp810_driver.h"

#include <stdio.h>

#include "configuration.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;
extern I2C_HandleTypeDef hi2c4;

#define SDP810_SENSOR_COUNT                              (4U)
#define SDP810_CMD_START_CONTINUOUS_DIFF_PRESSURE_AVG    (0x3615U)
#define SDP810_CMD_STOP_CONTINUOUS                        (0x3FF9U)
#define SDP810_READ_FRAME_SIZE                            (9U)

typedef struct
{
  I2C_HandleTypeDef *bus;
  uint8_t address;
} Sdp810Bus_t;

static const Sdp810Bus_t kSensorBuses[SDP810_SENSOR_COUNT] = {
  {&hi2c1, LOGGER_SENSOR_1_I2C_ADDRESS},
  {&hi2c2, LOGGER_SENSOR_2_I2C_ADDRESS},
  {&hi2c3, LOGGER_SENSOR_3_I2C_ADDRESS},
  {&hi2c4, LOGGER_SENSOR_4_I2C_ADDRESS}
};

static uint8_t Sdp810_CalculateCrc(const uint8_t *data, uint8_t count);
static Sdp810Status_t Sdp810_SendCommand(const Sdp810Bus_t *sensor, uint16_t command);
static Sdp810Status_t Sdp810_ReadMeasurement(const Sdp810Bus_t *sensor,
                                             Sdp810Measurement_t *measurement);

bool Sdp810Driver_Init(void)
{
  uint32_t index;
  Sdp810Status_t status;

  for (index = 0U; index < SDP810_SENSOR_COUNT; ++index)
  {
    (void)Sdp810_SendCommand(&kSensorBuses[index], SDP810_CMD_STOP_CONTINUOUS);
    HAL_Delay(1U);

    status = Sdp810_SendCommand(&kSensorBuses[index], SDP810_CMD_START_CONTINUOUS_DIFF_PRESSURE_AVG);
    if (status != SDP810_STATUS_OK)
    {
      printf("[SDP810] Start failed on sensor %lu bus=%p status=%d\r\n",
             (unsigned long)(index + 1U),
             (void *)kSensorBuses[index].bus,
             (int)status);
      return false;
    }

    printf("[SDP810] Sensor %lu started on bus=%p addr=0x%02X\r\n",
           (unsigned long)(index + 1U),
           (void *)kSensorBuses[index].bus,
           (unsigned int)kSensorBuses[index].address);
  }

  HAL_Delay(LOGGER_SENSOR_STARTUP_DELAY_MS);
  return true;
}

void Sdp810Driver_Stop(void)
{
  uint32_t index;

  for (index = 0U; index < SDP810_SENSOR_COUNT; ++index)
  {
    (void)Sdp810_SendCommand(&kSensorBuses[index], SDP810_CMD_STOP_CONTINUOUS);
    HAL_Delay(1U);
  }
}

void Sdp810Driver_ReadAll(Sdp810Measurement_t measurements[4])
{
  uint32_t index;

  for (index = 0U; index < SDP810_SENSOR_COUNT; ++index)
  {
    measurements[index].valid = false;
    measurements[index].differential_pressure_ticks = 0;
    measurements[index].temperature_ticks = 0;
    measurements[index].scale_factor = 0;
    measurements[index].status = Sdp810_ReadMeasurement(&kSensorBuses[index], &measurements[index]);
    measurements[index].valid = (measurements[index].status == SDP810_STATUS_OK);
  }
}

static uint8_t Sdp810_CalculateCrc(const uint8_t *data, uint8_t count)
{
  uint8_t crc = 0xFFU;
  uint8_t bit_index;
  uint8_t byte_index;

  for (byte_index = 0U; byte_index < count; ++byte_index)
  {
    crc ^= data[byte_index];
    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ 0x31U);
      }
      else
      {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

static Sdp810Status_t Sdp810_SendCommand(const Sdp810Bus_t *sensor, uint16_t command)
{
  uint8_t command_bytes[2];
  HAL_StatusTypeDef hal_status;

  command_bytes[0] = (uint8_t)(command >> 8U);
  command_bytes[1] = (uint8_t)(command & 0xFFU);

  hal_status = HAL_I2C_Master_Transmit(sensor->bus,
                                       (uint16_t)(sensor->address << 1U),
                                       command_bytes,
                                       sizeof(command_bytes),
                                       LOGGER_SENSOR_READ_TIMEOUT_MS);

  return (hal_status == HAL_OK) ? SDP810_STATUS_OK : SDP810_STATUS_I2C_TX_ERROR;
}

static Sdp810Status_t Sdp810_ReadMeasurement(const Sdp810Bus_t *sensor,
                                             Sdp810Measurement_t *measurement)
{
  uint8_t frame[SDP810_READ_FRAME_SIZE];
  HAL_StatusTypeDef hal_status;

  hal_status = HAL_I2C_Master_Receive(sensor->bus,
                                      (uint16_t)(sensor->address << 1U),
                                      frame,
                                      sizeof(frame),
                                      LOGGER_SENSOR_READ_TIMEOUT_MS);
  if (hal_status != HAL_OK)
  {
    return SDP810_STATUS_I2C_RX_ERROR;
  }

  if ((Sdp810_CalculateCrc(&frame[0], 2U) != frame[2]) ||
      (Sdp810_CalculateCrc(&frame[3], 2U) != frame[5]) ||
      (Sdp810_CalculateCrc(&frame[6], 2U) != frame[8]))
  {
    return SDP810_STATUS_CRC_ERROR;
  }

  measurement->differential_pressure_ticks = (int16_t)(((uint16_t)frame[0] << 8U) | frame[1]);
  measurement->temperature_ticks = (int16_t)(((uint16_t)frame[3] << 8U) | frame[4]);
  measurement->scale_factor = (int16_t)(((uint16_t)frame[6] << 8U) | frame[7]);

  if (measurement->scale_factor == 0)
  {
    return SDP810_STATUS_INVALID_SCALE;
  }

  return SDP810_STATUS_OK;
}
