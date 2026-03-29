#ifndef __SDP810_DRIVER_H
#define __SDP810_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  SDP810_STATUS_OK = 0U,
  SDP810_STATUS_I2C_TX_ERROR,
  SDP810_STATUS_I2C_RX_ERROR,
  SDP810_STATUS_CRC_ERROR,
  SDP810_STATUS_INVALID_SCALE
} Sdp810Status_t;

typedef struct
{
  bool valid;
  int16_t differential_pressure_ticks;
  int16_t temperature_ticks;
  int16_t scale_factor;
  Sdp810Status_t status;
} Sdp810Measurement_t;

bool Sdp810Driver_Init(void);
void Sdp810Driver_Stop(void);
void Sdp810Driver_ReadAll(Sdp810Measurement_t measurements[4]);

#ifdef __cplusplus
}
#endif

#endif /* __SDP810_DRIVER_H */
