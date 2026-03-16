#ifndef __FEEDBACK_H
#define __FEEDBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  FEEDBACK_COLOR_OFF = 0,
  FEEDBACK_COLOR_RED,
  FEEDBACK_COLOR_GREEN,
  FEEDBACK_COLOR_BLUE,
  FEEDBACK_COLOR_YELLOW,
  FEEDBACK_COLOR_CYAN,
  FEEDBACK_COLOR_MAGENTA,
  FEEDBACK_COLOR_WHITE,
  FEEDBACK_COLOR_ORANGE,
  FEEDBACK_COLOR_CUSTOM
} FeedbackColor_t;

typedef enum
{
  FEEDBACK_SOUND_NONE = 0,
  FEEDBACK_SOUND_BEEP,
  FEEDBACK_SOUND_STARTUP,
  FEEDBACK_SOUND_SUCCESS,
  FEEDBACK_SOUND_ERROR,
  FEEDBACK_SOUND_WARNING,
  FEEDBACK_SOUND_NOTIFICATION
} FeedbackSound_t;

typedef enum
{
  FEEDBACK_PATTERN_NONE = 0,
  FEEDBACK_PATTERN_STARTUP,
  FEEDBACK_PATTERN_SUCCESS,
  FEEDBACK_PATTERN_ERROR,
  FEEDBACK_PATTERN_WARNING,
  FEEDBACK_PATTERN_NOTIFICATION
} FeedbackPattern_t;

typedef struct
{
  uint16_t red;
  uint16_t green;
  uint16_t blue;
} FeedbackRgb_t;

void Feedback_Init(void);
void Feedback_Update(uint32_t now_ms);

void Feedback_SetColor(FeedbackColor_t color);
void Feedback_SetRGB(uint16_t red, uint16_t green, uint16_t blue);
FeedbackRgb_t Feedback_GetRGB(void);

void Feedback_PlaySound(FeedbackSound_t sound);
void Feedback_StopSound(void);

void Feedback_TriggerPattern(FeedbackPattern_t pattern);
void Feedback_StopPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* __FEEDBACK_H */
