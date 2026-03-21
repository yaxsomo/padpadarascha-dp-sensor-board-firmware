#include "feedback.h"
#include "configuration.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

#define FEEDBACK_LED_MAX_BRIGHTNESS        (4095U)
#define FEEDBACK_BUZZER_CHANNEL            TIM_CHANNEL_1
#define FEEDBACK_BUZZER_TIMER_CLOCK_HZ     (3000000UL)
#define FEEDBACK_BUZZER_MIN_COUNTS         (2UL)
#define FEEDBACK_PATTERN_STEP_TERMINATOR   (0U)

typedef struct
{
  uint16_t frequency_hz;
  uint16_t duration_ms;
} FeedbackToneStep_t;

typedef struct
{
  FeedbackColor_t color;
  FeedbackSound_t sound;
  uint16_t duration_ms;
} FeedbackPatternStep_t;

typedef struct
{
  FeedbackRgb_t current_rgb;
  FeedbackRgb_t custom_rgb;
  FeedbackColor_t current_color;
  FeedbackSound_t current_sound;
  FeedbackPattern_t current_pattern;
  uint32_t next_sound_tick_ms;
  uint32_t next_pattern_tick_ms;
  uint8_t sound_step_index;
  uint8_t pattern_step_index;
  uint8_t initialized;
  uint8_t buzzer_active;
  uint8_t rgb_active;
} FeedbackState_t;

static FeedbackState_t s_feedback;

static const FeedbackToneStep_t kSoundBeep[] = {
  {2400U, 80U},
  {0U, 0U}
};

static const FeedbackToneStep_t kSoundStartup[] = {
  {523U, 80U},
  {659U, 80U},
  {784U, 120U},
  {0U, 0U}
};

static const FeedbackToneStep_t kSoundSuccess[] = {
  {880U, 70U},
  {1175U, 120U},
  {0U, 0U}
};

static const FeedbackToneStep_t kSoundError[] = {
  {330U, 140U},
  {220U, 220U},
  {0U, 0U}
};

static const FeedbackToneStep_t kSoundWarning[] = {
  {988U, 90U},
  {0U, 60U},
  {988U, 90U},
  {0U, 0U}
};

static const FeedbackToneStep_t kSoundNotification[] = {
  {1319U, 60U},
  {1568U, 60U},
  {0U, 0U}
};

static const FeedbackPatternStep_t kPatternStartup[] = {
  {FEEDBACK_COLOR_BLUE, FEEDBACK_SOUND_STARTUP, 320U},
  {FEEDBACK_COLOR_GREEN, FEEDBACK_SOUND_NONE, 220U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, FEEDBACK_PATTERN_STEP_TERMINATOR}
};

static const FeedbackPatternStep_t kPatternSuccess[] = {
  {FEEDBACK_COLOR_GREEN, FEEDBACK_SOUND_SUCCESS, 240U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, FEEDBACK_PATTERN_STEP_TERMINATOR}
};

static const FeedbackPatternStep_t kPatternError[] = {
  {FEEDBACK_COLOR_RED, FEEDBACK_SOUND_ERROR, 260U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, 80U},
  {FEEDBACK_COLOR_RED, FEEDBACK_SOUND_NONE, 180U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, FEEDBACK_PATTERN_STEP_TERMINATOR}
};

static const FeedbackPatternStep_t kPatternWarning[] = {
  {FEEDBACK_COLOR_ORANGE, FEEDBACK_SOUND_WARNING, 180U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, 80U},
  {FEEDBACK_COLOR_ORANGE, FEEDBACK_SOUND_NONE, 180U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, FEEDBACK_PATTERN_STEP_TERMINATOR}
};

static const FeedbackPatternStep_t kPatternNotification[] = {
  {FEEDBACK_COLOR_CYAN, FEEDBACK_SOUND_NOTIFICATION, 180U},
  {FEEDBACK_COLOR_OFF, FEEDBACK_SOUND_NONE, FEEDBACK_PATTERN_STEP_TERMINATOR}
};

static uint16_t Feedback_ClampBrightness(uint16_t value);
static void Feedback_ApplyRgb(const FeedbackRgb_t *rgb);
static FeedbackRgb_t Feedback_GetColorRgb(FeedbackColor_t color);
static void Feedback_SetBuzzerFrequency(uint16_t frequency_hz);
static void Feedback_ApplySoundStep(uint32_t now_ms);
static void Feedback_ApplyPatternStep(uint32_t now_ms);
static const FeedbackToneStep_t *Feedback_GetSoundSequence(FeedbackSound_t sound);
static const FeedbackPatternStep_t *Feedback_GetPatternSequence(FeedbackPattern_t pattern);
static uint8_t Feedback_TimeReached(uint32_t now_ms, uint32_t target_ms);

void Feedback_Init(void)
{
  s_feedback.current_color = FEEDBACK_COLOR_OFF;
  s_feedback.current_sound = FEEDBACK_SOUND_NONE;
  s_feedback.current_pattern = FEEDBACK_PATTERN_NONE;
  s_feedback.custom_rgb.red = FEEDBACK_LED_MAX_BRIGHTNESS;
  s_feedback.custom_rgb.green = FEEDBACK_LED_MAX_BRIGHTNESS;
  s_feedback.custom_rgb.blue = FEEDBACK_LED_MAX_BRIGHTNESS;
  s_feedback.current_rgb = Feedback_GetColorRgb(FEEDBACK_COLOR_OFF);
  s_feedback.sound_step_index = 0U;
  s_feedback.pattern_step_index = 0U;
  s_feedback.next_sound_tick_ms = 0U;
  s_feedback.next_pattern_tick_ms = 0U;
  s_feedback.buzzer_active = 0U;
  s_feedback.rgb_active = 0U;

  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  if (BUZZER_OFF == CONFIG_STATE_OFF)
  {
    (void)HAL_TIM_PWM_Start(&htim2, FEEDBACK_BUZZER_CHANNEL);
  }

  Feedback_ApplyRgb(&s_feedback.current_rgb);
  Feedback_SetBuzzerFrequency(0U);

  s_feedback.initialized = 1U;
}

void Feedback_Update(uint32_t now_ms)
{
  if (s_feedback.initialized == 0U)
  {
    return;
  }

  if ((s_feedback.current_sound != FEEDBACK_SOUND_NONE) &&
      (Feedback_TimeReached(now_ms, s_feedback.next_sound_tick_ms) != 0U))
  {
    Feedback_ApplySoundStep(now_ms);
  }

  if ((s_feedback.current_pattern != FEEDBACK_PATTERN_NONE) &&
      (Feedback_TimeReached(now_ms, s_feedback.next_pattern_tick_ms) != 0U))
  {
    Feedback_ApplyPatternStep(now_ms);
  }
}

void Feedback_SetColor(FeedbackColor_t color)
{
  FeedbackRgb_t rgb;

  if (s_feedback.initialized == 0U)
  {
    return;
  }

  if (color != FEEDBACK_COLOR_CUSTOM)
  {
    s_feedback.current_color = color;
  }

  Feedback_StopPattern();
  rgb = Feedback_GetColorRgb(color);
  Feedback_ApplyRgb(&rgb);
}

void Feedback_SetRGB(uint16_t red, uint16_t green, uint16_t blue)
{
  FeedbackRgb_t rgb;

  if (s_feedback.initialized == 0U)
  {
    return;
  }

  Feedback_StopPattern();

  rgb.red = Feedback_ClampBrightness(red);
  rgb.green = Feedback_ClampBrightness(green);
  rgb.blue = Feedback_ClampBrightness(blue);

  s_feedback.custom_rgb = rgb;
  s_feedback.current_color = FEEDBACK_COLOR_CUSTOM;
  Feedback_ApplyRgb(&rgb);
}

FeedbackRgb_t Feedback_GetRGB(void)
{
  return s_feedback.current_rgb;
}

void Feedback_PlaySound(FeedbackSound_t sound)
{
  if (s_feedback.initialized == 0U)
  {
    return;
  }

  if (BUZZER_OFF == CONFIG_STATE_ON)
  {
    (void)sound;
    Feedback_StopSound();
    return;
  }

  s_feedback.current_sound = sound;
  s_feedback.sound_step_index = 0U;

  if (sound == FEEDBACK_SOUND_NONE)
  {
    Feedback_StopSound();
    return;
  }

  Feedback_ApplySoundStep(HAL_GetTick());
}

void Feedback_StopSound(void)
{
  if (s_feedback.initialized == 0U)
  {
    return;
  }

  s_feedback.current_sound = FEEDBACK_SOUND_NONE;
  s_feedback.sound_step_index = 0U;
  s_feedback.next_sound_tick_ms = 0U;
  s_feedback.buzzer_active = 0U;
  Feedback_SetBuzzerFrequency(0U);
}

void Feedback_TriggerPattern(FeedbackPattern_t pattern)
{
  if (s_feedback.initialized == 0U)
  {
    return;
  }

  if (pattern == FEEDBACK_PATTERN_NONE)
  {
    Feedback_StopPattern();
    return;
  }

  s_feedback.current_pattern = pattern;
  s_feedback.pattern_step_index = 0U;
  Feedback_ApplyPatternStep(HAL_GetTick());
}

void Feedback_StopPattern(void)
{
  s_feedback.current_pattern = FEEDBACK_PATTERN_NONE;
  s_feedback.pattern_step_index = 0U;
  s_feedback.next_pattern_tick_ms = 0U;
}

static uint16_t Feedback_ClampBrightness(uint16_t value)
{
  if (value > FEEDBACK_LED_MAX_BRIGHTNESS)
  {
    return FEEDBACK_LED_MAX_BRIGHTNESS;
  }

  return value;
}

static void Feedback_ApplyRgb(const FeedbackRgb_t *rgb)
{
  uint16_t red_compare;
  uint16_t green_compare;
  uint16_t blue_compare;

  s_feedback.current_rgb.red = Feedback_ClampBrightness(rgb->red);
  s_feedback.current_rgb.green = Feedback_ClampBrightness(rgb->green);
  s_feedback.current_rgb.blue = Feedback_ClampBrightness(rgb->blue);

  /* Common-anode RGB LED: lower duty cycle means higher LED current. */
  red_compare = FEEDBACK_LED_MAX_BRIGHTNESS - s_feedback.current_rgb.red;
  green_compare = FEEDBACK_LED_MAX_BRIGHTNESS - s_feedback.current_rgb.green;
  blue_compare = FEEDBACK_LED_MAX_BRIGHTNESS - s_feedback.current_rgb.blue;

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, red_compare);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, green_compare);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, blue_compare);

  s_feedback.rgb_active = (s_feedback.current_rgb.red != 0U) ||
                          (s_feedback.current_rgb.green != 0U) ||
                          (s_feedback.current_rgb.blue != 0U);
}

static FeedbackRgb_t Feedback_GetColorRgb(FeedbackColor_t color)
{
  FeedbackRgb_t rgb = {0U, 0U, 0U};
  const uint16_t full = FEEDBACK_LED_MAX_BRIGHTNESS;
  const uint16_t half = FEEDBACK_LED_MAX_BRIGHTNESS / 2U;
  const uint16_t quarter = FEEDBACK_LED_MAX_BRIGHTNESS / 4U;

  switch (color)
  {
    case FEEDBACK_COLOR_RED:
      rgb.red = full;
      break;

    case FEEDBACK_COLOR_GREEN:
      rgb.green = full;
      break;

    case FEEDBACK_COLOR_BLUE:
      rgb.blue = full;
      break;

    case FEEDBACK_COLOR_YELLOW:
      rgb.red = full;
      rgb.green = full;
      break;

    case FEEDBACK_COLOR_CYAN:
      rgb.green = full;
      rgb.blue = full;
      break;

    case FEEDBACK_COLOR_MAGENTA:
      rgb.red = full;
      rgb.blue = full;
      break;

    case FEEDBACK_COLOR_WHITE:
      rgb.red = full;
      rgb.green = full;
      rgb.blue = full;
      break;

    case FEEDBACK_COLOR_ORANGE:
      rgb.red = full;
      rgb.green = quarter + half;
      break;

    case FEEDBACK_COLOR_CUSTOM:
      rgb = s_feedback.custom_rgb;
      break;

    case FEEDBACK_COLOR_OFF:
    default:
      break;
  }

  return rgb;
}

static void Feedback_SetBuzzerFrequency(uint16_t frequency_hz)
{
  uint32_t counts;
  uint32_t auto_reload;

  if (BUZZER_OFF == CONFIG_STATE_ON)
  {
    (void)frequency_hz;
    __HAL_TIM_SET_COMPARE(&htim2, FEEDBACK_BUZZER_CHANNEL, 0U);
    return;
  }

  if (frequency_hz == 0U)
  {
    __HAL_TIM_SET_COMPARE(&htim2, FEEDBACK_BUZZER_CHANNEL, 0U);
    return;
  }

  counts = FEEDBACK_BUZZER_TIMER_CLOCK_HZ / (uint32_t)frequency_hz;
  if (counts < FEEDBACK_BUZZER_MIN_COUNTS)
  {
    counts = FEEDBACK_BUZZER_MIN_COUNTS;
  }

  auto_reload = counts - 1U;
  __HAL_TIM_SET_AUTORELOAD(&htim2, auto_reload);
  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_SET_COMPARE(&htim2, FEEDBACK_BUZZER_CHANNEL, counts / 2U);
}

static void Feedback_ApplySoundStep(uint32_t now_ms)
{
  const FeedbackToneStep_t *sequence;
  const FeedbackToneStep_t *step;

  sequence = Feedback_GetSoundSequence(s_feedback.current_sound);
  if (sequence == NULL)
  {
    Feedback_StopSound();
    return;
  }

  step = &sequence[s_feedback.sound_step_index];
  if (step->duration_ms == 0U)
  {
    Feedback_StopSound();
    return;
  }

  Feedback_SetBuzzerFrequency(step->frequency_hz);
  s_feedback.buzzer_active = (step->frequency_hz != 0U);
  s_feedback.next_sound_tick_ms = now_ms + step->duration_ms;
  s_feedback.sound_step_index++;
}

static void Feedback_ApplyPatternStep(uint32_t now_ms)
{
  const FeedbackPatternStep_t *sequence;
  const FeedbackPatternStep_t *step;
  FeedbackRgb_t rgb;

  sequence = Feedback_GetPatternSequence(s_feedback.current_pattern);
  if (sequence == NULL)
  {
    Feedback_StopPattern();
    return;
  }

  step = &sequence[s_feedback.pattern_step_index];
  if (step->duration_ms == FEEDBACK_PATTERN_STEP_TERMINATOR)
  {
    Feedback_StopPattern();
    return;
  }

  rgb = Feedback_GetColorRgb(step->color);
  s_feedback.current_color = step->color;
  Feedback_ApplyRgb(&rgb);

  if (step->sound != FEEDBACK_SOUND_NONE)
  {
    Feedback_PlaySound(step->sound);
  }
  else
  {
    Feedback_StopSound();
  }

  s_feedback.next_pattern_tick_ms = now_ms + step->duration_ms;
  s_feedback.pattern_step_index++;
}

static const FeedbackToneStep_t *Feedback_GetSoundSequence(FeedbackSound_t sound)
{
  switch (sound)
  {
    case FEEDBACK_SOUND_BEEP:
      return kSoundBeep;

    case FEEDBACK_SOUND_STARTUP:
      return kSoundStartup;

    case FEEDBACK_SOUND_SUCCESS:
      return kSoundSuccess;

    case FEEDBACK_SOUND_ERROR:
      return kSoundError;

    case FEEDBACK_SOUND_WARNING:
      return kSoundWarning;

    case FEEDBACK_SOUND_NOTIFICATION:
      return kSoundNotification;

    case FEEDBACK_SOUND_NONE:
    default:
      return NULL;
  }
}

static const FeedbackPatternStep_t *Feedback_GetPatternSequence(FeedbackPattern_t pattern)
{
  switch (pattern)
  {
    case FEEDBACK_PATTERN_STARTUP:
      return kPatternStartup;

    case FEEDBACK_PATTERN_SUCCESS:
      return kPatternSuccess;

    case FEEDBACK_PATTERN_ERROR:
      return kPatternError;

    case FEEDBACK_PATTERN_WARNING:
      return kPatternWarning;

    case FEEDBACK_PATTERN_NOTIFICATION:
      return kPatternNotification;

    case FEEDBACK_PATTERN_NONE:
    default:
      return NULL;
  }
}

static uint8_t Feedback_TimeReached(uint32_t now_ms, uint32_t target_ms)
{
  return ((int32_t)(now_ms - target_ms) >= 0) ? 1U : 0U;
}

/*
Example usage:

  #include "feedback.h"

  Feedback_Init();
  Feedback_TriggerPattern(FEEDBACK_PATTERN_STARTUP);

  while (1)
  {
    Feedback_Update(HAL_GetTick());

    if (app_ok != 0U)
    {
      Feedback_SetColor(FEEDBACK_COLOR_GREEN);
      Feedback_PlaySound(FEEDBACK_SOUND_SUCCESS);
    }

    if (app_error != 0U)
    {
      Feedback_TriggerPattern(FEEDBACK_PATTERN_ERROR);
    }
  }

  Feedback_SetRGB(4095U, 512U, 0U);
  Feedback_PlaySound(FEEDBACK_SOUND_BEEP);
*/
