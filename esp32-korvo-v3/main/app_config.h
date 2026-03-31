{
    "name": "esp32_conversation",
    "properties": {
        "channel": "esp32_conversation",
        "agent_rtc_uid": "1234",
        "token": "",
        "remote_rtc_uids": [
            "5678"
        ],
        "enable_string_uid": false,
        "idle_timeout": 120,
        "advanced_features": {
          "enable_aivad": false,
          "enable_bhvs": true,
          "enable_tools": true
        },
        "parameters": {
          "output_audio_codec": "PCMU",
          "agent_rtc_sdk_params": "{\"che.audio.acm_ptime\": 20}",
          "enable_dump": true,
          "transcript": {
            "enable": false,
            "redundant": false,
            "enable_words": false
          },
          "data_channel": "datastream"
        },
        "llm": {
            "url": "YOUR LLM URL",
            "api_key": "YOUR  LLM API KEY",
            "input_modalities":["text", "image"],
            "predefined_tools": ["_publish_message"],
            "system_messages": [
                {
                    "role": "system",
                    "content": "you are a helpful robot!"
                }
            ],
            "greeting_message": "Hello, how can I assist you today?",
            "failure_message": "Please hold on a second.",
            "params": {
                "model": "gpt-4o"
            }
        },
        "tts": {
            "vendor": "YOUR TTS VENDOR",
            "params": {
                "key": "YOUR TTS KEY",
                "voice_name": "TTS VOICE NAME"
            }
        }
    }
}
