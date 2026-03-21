#ifndef __CONFIGURATION_H
#define __CONFIGURATION_H

typedef enum
{
  CONFIG_STATE_OFF = 0U,
  CONFIG_STATE_ON
} ConfigurationState_t;

/*
 * Set to CONFIG_STATE_ON to force the buzzer off during board bring-up and LED
 * testing. Set to CONFIG_STATE_OFF to enable normal buzzer playback.
 */
#define BUZZER_OFF    (CONFIG_STATE_ON)

#endif /* __CONFIGURATION_H */
