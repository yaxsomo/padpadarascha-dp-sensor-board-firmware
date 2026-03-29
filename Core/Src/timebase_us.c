#include "timebase_us.h"

#include "main.h"

static uint32_t s_last_cycle_count;
static uint64_t s_accumulated_cycles;
static uint32_t s_cpu_hz;
static uint8_t s_initialized;

void Timebase_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0U;

  s_last_cycle_count = 0U;
  s_accumulated_cycles = 0U;
  s_cpu_hz = HAL_RCC_GetHCLKFreq();
  s_initialized = 1U;
}

uint64_t Timebase_GetMicros(void)
{
  uint32_t current_cycles;
  uint32_t delta_cycles;

  if (s_initialized == 0U)
  {
    Timebase_Init();
  }

  current_cycles = DWT->CYCCNT;
  delta_cycles = current_cycles - s_last_cycle_count;
  s_last_cycle_count = current_cycles;
  s_accumulated_cycles += (uint64_t)delta_cycles;

  if (s_cpu_hz == 0U)
  {
    return 0U;
  }

  return (s_accumulated_cycles * 1000000ULL) / (uint64_t)s_cpu_hz;
}
