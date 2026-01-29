# ESP32 Conversational AI Clients

  This repository contains multiple ESP32-based client implementations for Agora Conversational AI Agent API v2.

  ## Projects

  ### 1. [ReSpeaker XVF3800](./esp32-respeaker-convo/)
  ESP32-S3 client for ReSpeaker 4-Mic Array with XVF3800.

  **Hardware:**
  - Seeed Studio ReSpeaker 4-Mic Array with XVF3800
  - Seeed Studio XIAO ESP32-S3

  **Features:**
  - XVF3800 button control
  - AIC3104 audio codec
  - 4-microphone array

  [→ View Documentation](./esp32-respeaker-convo/README.md)

  ---

  ### 2. [ESP32-S3-Korvo-2 V3](./esp32-korvo-v3-convo/)
  ESP32-S3 client for ESP32-S3-Korvo-2 V3 development board.

  **Hardware:**
  - Espressif ESP32-S3-Korvo-2 V3

  **Features:**
  - Built-in buttons (SET, MUTE, VOL+, VOL-)
  - ES8311 audio codec
  - Dual microphones

  [→ View Documentation](./esp32-korvo-v3-convo/README.md)

  ---

  ### 3. [ReSpeaker XVF3800](./esp32-respeaker-ten/)
  ESP32-S3 client for ReSpeaker 4-Mic Array with XVF3800.

  **Hardware:**
  - Seeed Studio ReSpeaker 4-Mic Array with XVF3800
  - Seeed Studio XIAO ESP32-S3

  **Features:**
  - XVF3800 button control
  - AIC3104 audio codec
  - 4-microphone array

  [→ View Documentation](./esp32-respeaker-ten/README.md)

  ---

  ## Quick Start

  Each project has its own README with detailed setup instructions. Navigate to the project folder and follow the instructions:

  ```bash
  cd esp32-respeaker-convo/
  # Follow setup instructions in README.md

  Common Requirements

  All projects require:
  - ESP-IDF v5.2.3
  - ESP-ADF v2.7
  - Agora App ID and credentials
  - LLM API key (OpenAI, Azure, etc.)
  - TTS service credentials

  Resources

  - https://console.agora.io/
  - https://docs.agora.io/en/conversational-ai
  - https://docs.espressif.com/projects/esp-idf/en/v5.2.3/
  - https://docs.espressif.com/projects/esp-adf/en/latest/

  License

  Each project may have its own license. See individual project folders for details.

