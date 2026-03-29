#include "debug_output.h"

#include "configuration.h"
#include "main.h"

extern UART_HandleTypeDef huart4;

void DebugOutput_Init(void)
{
}

int DebugOutput_Write(const char *data, int length)
{
  int index;

  if ((data == NULL) || (length <= 0))
  {
    return 0;
  }

  switch (DEBUG_PRINT_INTERFACE)
  {
    case DEBUG_PRINT_INTERFACE_UART4:
      if (HAL_UART_Transmit(&huart4, (uint8_t *)data, (uint16_t)length, HAL_MAX_DELAY) != HAL_OK)
      {
        return 0;
      }
      break;

    case DEBUG_PRINT_INTERFACE_SWV:
    default:
      for (index = 0; index < length; ++index)
      {
        ITM_SendChar((uint32_t)data[index]);
      }
      break;
  }

  return length;
}
