#ifndef APP_COMMON_H
#define APP_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "project_config.h"

#define RTC_APP_ID_LEN   64
#define RTC_TOKEN_LEN    512
#define WIFI_SSID_LEN    64
#define WIFI_PASSWORD_LEN 64
#define AI_AGENT_NAME_LEN 128
#define AI_AGENT_CHANNEL_LEN 128

#define AUDIO_I2S_BITS   32
#define PRIO_TASK_FETCH (21)

#define CONFIG_PCM_CHANNEL_NUM (1)


typedef struct {
  bool b_wifi_connected;
  bool b_ai_agent_generated;
  bool b_call_session_started;
  bool b_ai_agent_joined;

  char app_id[RTC_APP_ID_LEN];
  char token[RTC_TOKEN_LEN];
  char wifi_ssid[WIFI_SSID_LEN];
  char wifi_password[WIFI_PASSWORD_LEN];
  char agent_id[128];  // Store agent ID from start response
  char agent_name[AI_AGENT_NAME_LEN];
  char channel_name[AI_AGENT_CHANNEL_LEN];
  uint32_t agent_rtc_uid;
  uint32_t remote_rtc_uid;
  int rtc_audio_codec_type;
  int pcm_sample_rate;
  int pcm_data_len;
	
} app_t;

extern app_t g_app;

#ifdef __cplusplus
}
#endif
#endif
