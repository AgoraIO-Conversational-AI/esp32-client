# Agora Conversational AI Agent on ESP32-S3-Korvo-2 V3

*English | [简体中文](./README.cn.md)*

## Overview

This is an ESP32-S3 based real-time voice conversation client running on the **ESP32-S3-Korvo-2 V3** development board. The project enables natural voice conversations with AI agents through **Agora RTC** platform and **Agora Conversational AI Agent API v2**.

The board joins an RTC channel and communicates with an AI agent managed by Agora's Conversational AI service. The AI agent provides speech-to-text (ASR), large language model (LLM) processing, and text-to-speech (TTS) capabilities - all controlled via simple RESTful API calls from the ESP32.

### Key Features

- ✅ **Real-time Voice Conversation** - Low-latency audio streaming via Agora RTC
- ✅ **Simple AI Agent Control** - Start/Stop agent via RESTful API calls
- ✅ **On-board Button Control** - Physical buttons (SET, MUTE, VOL+, VOL-) for easy operation
- ✅ **AEC (Acoustic Echo Cancellation)** - Built-in echo cancellation for better audio quality
- ✅ **G.711 μ-law Audio** - Efficient audio codec for embedded systems
- ✅ **Configurable AI Backend** - Support for OpenAI, Azure, and other LLM providers
- ✅ **No Server Required** - ESP32 directly calls Agora Conversational AI API

### Hardware Platform

This project is specifically designed for:
- **Board**: ESP32-S3-Korvo-2 V3
- **Microcontroller**: ESP32-S3 with 8MB PSRAM
- **Audio Codec**: ES8311 codec with ES7210 ADC
- **Microphones**: Dual on-board microphones

## Table of Contents

- [How It Works](#how-it-works)
- [Hardware Requirements](#hardware-requirements)
- [File Structure](#file-structure)
- [Environment Setup](#environment-setup)
- [Project Configuration](#project-configuration)
- [Compilation and Flashing](#compilation-and-flashing)
- [Usage Guide](#usage-guide)
- [Project Architecture](#project-architecture)
- [Agora Conversational AI API Reference](#agora-conversational-ai-api-reference)
- [About Agora](#about-agora)

---

## How It Works

This project uses **Agora Conversational AI Agent API v2** to manage AI agents through simple RESTful API calls. Here's the complete workflow:

### System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ESP32-S3-Korvo-2 V3 Board                                │
│                                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────────────────┐    │
│  │   Buttons    │───►│  llm_main.c  │───►│     ai_agent.c           │    │
│  │ SET/MUTE/VOL │    │              │    │  (HTTP Client)           │    │
│  └──────────────┘    └──────────────┘    │                          │    │
│                                           │  - POST /join (start)    │    │
│                                           │  - POST /leave (stop)    │    │
│                                           └──────────┬───────────────┘    │
│                                                      │ HTTPS                │
│  ┌──────────────┐    ┌──────────────┐              │                      │
│  │ Microphones  │───►│ audio_proc.c │              │                      │
│  │  (ES7210)    │    │              │              │                      │
│  └──────────────┘    └──────┬───────┘              │                      │
│                              │                      │                      │
│  ┌──────────────┐    ┌──────▼───────┐              │                      │
│  │   Speaker    │◄───│  rtc_proc.c  │              │                      │
│  │   (ES8311)   │    │              │              │                      │
│  └──────────────┘    └──────┬───────┘              │                      │
│                              │ RTC Audio (G.711)    │                      │
└──────────────────────────────┼──────────────────────┼──────────────────────┘
                               │                      │
                               │                      │
                          WiFi │                      │ HTTPS
                               │                      │
        ┌──────────────────────┼──────────────────────┼─────────────┐
        │                      │                      │             │
        │                      ▼                      ▼             │
        │          ┌────────────────────┐  ┌──────────────────┐    │
        │          │   Agora SD-RTN     │  │  Agora RESTful   │    │
        │          │   (RTC Network)    │  │  API Server      │    │
        │          │                    │  │                  │    │
        │          │  - Audio Stream    │  │  POST /join      │    │
        │          │    Transport       │  │  POST /leave     │    │
        │          │  - Low Latency     │  │                  │    │
        │          └─────────┬──────────┘  └────────┬─────────┘    │
        │                    │                      │               │
        │                    │                      │               │
        │          ┌─────────▼──────────────────────▼─────────┐    │
        │          │  Agora Conversational AI Agent Service  │    │
        │          │                                          │    │
        │          │  ┌────────────────────────────────────┐ │    │
        │          │  │   AI Agent (per channel)           │ │    │
        │          │  │                                    │ │    │
        │          │  │   ┌─────────┐  ┌──────┐  ┌─────┐ │ │    │
        │          │  │   │   ASR   │─►│ LLM  │─►│ TTS │ │ │    │
        │          │  │   │(Speech  │  │Chat  │  │Voice│ │ │    │
        │          │  │   │ to Text)│  │ GPT  │  │ Out │ │ │    │
        │          │  │   └─────────┘  └──────┘  └─────┘ │ │    │
        │          │  │                                    │ │    │
        │          │  │   Configuration from /join API:   │ │    │
        │          │  │   - LLM: OpenAI/Azure/etc         │ │    │
        │          │  │   - TTS: Microsoft/Cartesia/etc   │ │    │
        │          │  │   - ASR: Language settings        │ │    │
        │          │  └────────────────────────────────────┘ │    │
        │          └───────────────────────────────────────────┘    │
        │                      Agora Cloud                          │
        └───────────────────────────────────────────────────────────┘
```

### Workflow Sequence

```
ESP32 Board          Agora API Server       Agora RTC Network      AI Agent Service
     │                      │                       │                      │
     │  1. User presses     │                       │                      │
     │     SET button       │                       │                      │
     ├─────────────────────►│                       │                      │
     │  2. POST /join       │                       │                      │
     │     {                │                       │                      │
     │       "name": "...", │                       │                      │
     │       "channel": "X",│                       │                      │
     │       "llm": {...},  │                       │                      │
     │       "tts": {...},  │                       │                      │
     │       "asr": {...}   │                       │                      │
     │     }                │                       │                      │
     │◄─────────────────────│                       │                      │
     │  3. HTTP 200 OK      │                       │                      │
     │     {agent_id: "xx"} │                       │                      │
     │                      │                       │                      │
     │                      ├──────────────────────────────────────────────►│
     │                      │  4. Create AI Agent with LLM/TTS/ASR config  │
     │                      │                       │                      │
     │                      │                       │◄─────────────────────│
     │                      │  5. Agent joins channel "X" (UID: 1001)      │
     │                      │                       │                      │
     ├──────────────────────────────────────────────►│                      │
     │  6. Board joins channel "X" (UID: 1002)      │                      │
     │                      │                       │                      │
     │◄─────────────────────────────────────────────┤                      │
     │  7. RTC Connected    │                       │                      │
     │                      │                       │                      │
     │══════════════════════════════════════════════╬══════════════════════│
     │  8. User speaks      │                       │                      │
     ├──────────────────────────────────────────────►│─────────────────────►│
     │      Audio Stream (G.711, 8kHz)              │   Audio to ASR       │
     │                      │                       │                      │
     │                      │                       │        ┌─────────────┤
     │                      │                       │        │ 9. ASR→LLM  │
     │                      │                       │        │    Processing│
     │                      │                       │        └─────────────►│
     │                      │                       │                      │
     │◄─────────────────────────────────────────────┤◄─────────────────────│
     │  10. AI Response Audio (TTS generated)       │   LLM→TTS→Audio      │
     │      Playback through speaker                │                      │
     │══════════════════════════════════════════════╬══════════════════════│
     │                      │                       │                      │
     │  11. User presses    │                       │                      │
     │      MUTE button     │                       │                      │
     ├─────────────────────►│                       │                      │
     │  12. POST /leave     │                       │                      │
     │      {agent_id: "xx"}│                       │                      │
     │◄─────────────────────│                       │                      │
     │  13. HTTP 200 OK     │                       │                      │
     │                      │                       │                      │
     │                      ├──────────────────────────────────────────────►│
     │                      │  14. Stop & Destroy AI Agent                 │
     │                      │                       │                      │
     ├──────────────────────────────────────────────►│                      │
     │  15. Leave RTC channel                       │                      │
     │                      │                       │                      │
```

### Key Components

1. **ESP32 Application** (`main/ai_agent.c`)
   - Makes HTTP POST requests to Agora Conversational AI API
   - **Start Agent**: `POST /projects/{appid}/agents/join` with LLM/TTS/ASR configuration
   - **Stop Agent**: `POST /projects/{appid}/agents/leave` with agent ID

2. **Agora IoT SDK** (`components/agora_iot_sdk/`)
   - Provides the embedded Agora RTC SDK used by the device
   - Handles RTC channel join/leave and real-time audio transport on ESP32

3. **Agora Conversational AI Service** (Cloud)
   - Receives RESTful API calls from ESP32
   - Creates/destroys AI agents dynamically per channel
   - Each agent includes: ASR (speech-to-text) → LLM (conversation) → TTS (text-to-speech)

4. **Agora RTC Network** (Cloud)
   - Transports real-time audio between ESP32 board and AI agent
   - Uses G.711 μ-law codec at 8kHz sample rate

---

## Hardware Requirements

### Required Components

1. **ESP32-S3-Korvo-2 V3 Development Board**
   - Product: [Espressif ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)
   - Features: Dual microphones, ES8311 codec, ES7210 ADC, LCD display

2. **Speaker**
   - Connect to the speaker output on the board
   - Recommended: 4Ω 3W speaker

3. **USB Cable**
   - USB-C cable for programming and power

### Hardware Interfaces

The ESP32-S3-Korvo-2 V3 has integrated:

| Interface | Function | Description |
|-----------|----------|-------------|
| **I2C** | Codec control | ES8311, ES7210 control |
| **I2S** | Audio data | High-quality audio streaming |
| **Buttons** | User control | SET, MUTE, VOL+, VOL- buttons |
| **USB** | Programming & Power | USB-C connection |

---

## File Structure

```
esp32-korvo-v3-convo/
├── CMakeLists.txt
├── components/
│   └── agora_iot_sdk/                  Agora IoT SDK
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── agora_rtc_api.h
│       └── libs/                       Pre-compiled libraries
│           ├── libagora-cjson.a
│           ├── libahpl.a
│           └── librtsa.a
├── main/                               Application code
│   ├── app_config.h                    Embedded AI Agent JSON config
│   ├── ai_agent.c                      AI Agent API client (HTTP POST /join, /leave)
│   ├── ai_agent.h
│   ├── audio_proc.c                    Audio pipeline (I2S, AEC)
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC handling (join/leave channel)
│   ├── rtc_proc.h
│   ├── llm_main.c                      Main application (WiFi, button control)
│   ├── common.h
│   ├── CMakeLists.txt
│   ├── project_config.h                Local credentials and WiFi config
│   └── Kconfig.projbuild
├── partitions.csv                      Flash partition table
├── sdkconfig.defaults                  ESP-IDF default config
├── sdkconfig.defaults.esp32s3          ESP32-S3 specific config
├── .gitignore
└── README.md
```

---

## Environment Setup

### Prerequisites

- **Operating System**: Linux (Ubuntu 20.04+), macOS, or Windows
- **ESP-IDF**: v5.2.3 (commitId: c9763f62dd00c887a1a8fafe388db868a7e44069)
- **ESP-ADF**: v2.7 (commitId: 9cf556de500019bb79f3bb84c821fda37668c052)
- **Python**: 3.8+

### Step 1: Install ESP-IDF v5.2.3

**Linux/macOS:**
```bash
# Create esp directory
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF
git clone -b v5.2.3 --recursive https://github.com/espressif/esp-idf.git

# Install dependencies
cd esp-idf
./install.sh esp32s3

# Set up environment variables (add to ~/.bashrc or ~/.zshrc)
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

**Windows:**
- Download IDF v5.2.3 (offline installer) from: [ESP-IDF Windows Setup](https://docs.espressif.com/projects/esp-idf/en/v5.2.3/esp32/get-started/windows-setup.html)
- Follow the installation wizard

### Step 2: Install ESP-ADF v2.7

**Linux/macOS:**
```bash
cd ~/esp

# Clone ESP-ADF
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# Set environment variable
export ADF_PATH=~/esp/esp-adf
echo 'export ADF_PATH=~/esp/esp-adf' >> ~/.bashrc  # or ~/.zshrc
```

**Windows:**
```bash
# Download ADF to Espressif/frameworks directory
cd C:\Espressif\frameworks
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# Set ADF_PATH environment variable
setx ADF_PATH C:\Espressif\frameworks\esp-adf
```

### Step 3: Apply IDF Patch

**Linux/macOS:**
```bash
cd ~/esp/esp-idf
git apply $ADF_PATH/idf_patches/idf_v5.2_freertos.patch
```

**Windows:**
```bash
cd %IDF_PATH%
git apply %ADF_PATH%/idf_patches/idf_v5.2_freertos.patch
```

### Step 4: Download Agora IoT SDK

```bash
cd /path/to/esp32-korvo-v3-convo/components

# Download and extract Agora IoT SDK
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
mkdir -p agora_iot_sdk && tar -xf agora_iot_sdk.tar -C agora_iot_sdk
```

After extraction, verify the structure:
```
components/agora_iot_sdk/
├── CMakeLists.txt
├── include/
│   └── agora_rtc_api.h
└── libs/
    ├── libagora-cjson.a
    ├── libahpl.a
    └── librtsa.a
```

---

## Project Configuration

This project now uses **two configuration files**:

- `main/project_config.h`: compile-time local settings used by the firmware itself, including Agora credentials, RTC App ID/token, and WiFi.
- `main/app_config.h`: embedded JSON payload sent to the Agora Conversational AI Agent `/join` API, including agent name, channel, LLM/ASR/TTS, and audio parameters.

### Step 1: Edit `main/project_config.h`

Update the local project-level macros first:

```c
#pragma once

#define AGORA_AI_AGENT_API_URL "https://api.agora.io/api/conversational-ai-agent/v2/projects"
#define AGORA_API_KEY          "YOUR_AGORA_API_KEY"
#define AGORA_API_SECRET       "YOUR_AGORA_API_SECRET"

#define AGORA_APP_ID           "YOUR_AGORA_APP_ID"
#define AGORA_RTC_TOKEN        ""   // Optional: provide RTC token if your project requires it

#define WIFI_SSID              "YOUR_WIFI_SSID"
#define WIFI_PASSWORD          "YOUR_WIFI_PASSWORD"
```

Notes:

1. `AGORA_API_KEY` and `AGORA_API_SECRET` are used for the HTTP Basic Auth header when calling the Conversational AI Agent REST API.
2. `AGORA_APP_ID` and `AGORA_RTC_TOKEN` are copied into the RTC runtime config in `llm_main.c`.

### Step 2: Edit `main/app_config.h`

`main/app_config.h` is a JSON document embedded into the firmware by `EMBED_TXTFILES` and sent as the AI Agent configuration body.

At minimum, make sure the following fields are correct:

```json
{
  "name": "your_agent_name",
  "properties": {
    "channel": "your_channel_name",
    "agent_rtc_uid": "1000",
    "remote_rtc_uids": ["12345"],
    "parameters": {
      "output_audio_codec": "PCMU"
    },
    "llm": {
      "url": "https://your-llm-endpoint",
      "api_key": "YOUR_LLM_API_KEY",
      "greeting_message": "Hello",
      "params": {
        "model": "YOUR_MODEL"
      }
    },
    "asr": {
      "language": "auto"
    },
    "tts": {
      "vendor": "null",
      "enable": false
    }
  }
}
```

Key fields:

1. `name`: Agora agent name.
2. `properties.channel`: RTC channel used by both the board and the cloud agent.
3. `properties.agent_rtc_uid`: RTC UID for the cloud AI agent.
4. `properties.remote_rtc_uids[0]`: RTC UID for the board/device. This must match the UID the device uses as the remote target.
5. `properties.parameters.output_audio_codec`: required by the firmware parser. Currently supported values are `PCMU` and `G722`.
6. `properties.llm` / `properties.asr` / `properties.tts`: provider-specific model and API settings sent directly to Agora when starting the agent.

**How to get your Agora credentials:**

1. Sign up or log in to [Agora Console](https://console.agora.io/)
2. Create a new project or select an existing project
3. Copy your **App ID** from the project dashboard
4. Enable **Conversational AI** service in your project settings
5. Generate **API credentials** (API Key and Secret) for Conversational AI Agent API v2

For detailed instructions, see: [Manage Agora Account - Conversational AI](https://docs.agora.io/en/conversational-ai/get-started/manage-agora-account)

> **Note:** The Conversational AI feature must be enabled in your Agora project before you can use the REST API.

---

## Compilation and Flashing

### Step 1: Set Up Environment

**Linux/macOS:**
```bash
# Load ESP-IDF environment
get_idf

# Or if you didn't create the alias:
. $HOME/esp/esp-idf/export.sh

# Set ADF_PATH (if not in .bashrc/.zshrc)
export ADF_PATH=~/esp/esp-adf
```

**Windows:**
```bash
# Open ESP-IDF PowerShell or CMD
# ADF_PATH should already be set in system environment
```

### Step 2: Navigate to Project

```bash
cd /path/to/esp32-korvo-v3-convo
```

### Step 3: Configure Target

```bash
idf.py set-target esp32s3
```

### Step 4: Review Optional Project Settings

WiFi is configured in `main/project_config.h`, so `idf.py menuconfig` is no longer required for SSID/password setup.

### Step 5: Build

```bash
idf.py build
```

**Build Output:**
You should see:
```
Project build complete. To flash, run:
  idf.py flash
```

### Step 6: Flash to Device

**Linux/macOS:**
```bash
# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Or on macOS (usually shows as)
idf.py -p /dev/cu.usbserial-* flash monitor
```

**Windows:**
```bash
idf.py -p COM3 flash monitor
```

---

## Usage Guide

### Quick Start

1. **Power On**
   - Connect ESP32-S3-Korvo-2 V3 via USB-C
   - Device will boot and connect to WiFi
   - Wait for message: `Press [SET] key to Join the Ai Agent ...`

2. **Start Conversation**
   - Press **SET button** once
   - Board sends `POST /join` request to Agora API
   - AI agent is created and joins RTC channel
   - Board joins the same RTC channel
   - Start speaking when you hear the greeting!

3. **Stop Conversation**
   - Press **MUTE button**
   - Board sends `POST /leave` request to Agora API
   - AI agent is destroyed
   - Board leaves RTC channel

4. **Volume Control**
   - Press **VOL+** button to increase volume (increments of 10, max 100)
   - Press **VOL-** button to decrease volume (decrements of 10, min 0)

### Button Functions

| Button | Function | API Call |
|--------|----------|----------|
| **SET** | Start AI agent | POST /join |
| **MUTE** | Stop AI agent | POST /leave |
| **VOL+** | Increase volume | - |
| **VOL-** | Decrease volume | - |

### Serial Monitor Output

```
I (1234) WIFI: Connected to WiFi
I (1235) WIFI: Got IP: 192.168.1.100
I (1236) AGORA: Press [SET] key to Join the Ai Agent ...
I (5000) AGORA: Starting AI agent...
I (5100) AGORA: POST /projects/7aaa.../agents/join
I (5200) AGORA: Agent started successfully (HTTP 200)
I (5300) AGORA: Joining RTC channel: test_channel
I (5500) AGORA: Successfully joined RTC channel
```

---

## Project Architecture

### Audio Pipeline

```
Microphones (ES7210 ADC)
    ↓ I2S
I2S Stream Reader
    ↓
Algorithm Stream (AEC)
    ↓
Raw Stream
    ↓
RTC Encoder (G.711 μ-law)
    ↓
Agora RTC → AI Agent (Cloud)
    ↓
    ASR (Speech-to-Text)
    ↓
    LLM (Conversation Processing)
    ↓
    TTS (Text-to-Speech)
    ↓
Agora RTC ← Audio Response
    ↓
RTC Decoder
    ↓
Raw Stream
    ↓
I2S Stream Writer
    ↓ I2S
Speaker (ES8311 Codec)
```

## Agora Conversational AI API Reference

### API Documentation

- [Agora Conversational AI - Join API](https://docs.agora.io/en/conversational-ai/rest-api/agent/join)
- [Agora Conversational AI - Leave API](https://docs.agora.io/en/conversational-ai/rest-api/agent/leave)

---

## About Agora

Agora provides real-time engagement APIs that power billions of minutes of live voice, video, and interactive streaming experiences worldwide. The Agora IoT SDK and Conversational AI service bring these capabilities to embedded devices like ESP32.

### Resources

- [Agora Console](https://console.agora.io/)
- [Agora Documentation](https://docs.agora.io/)
- [Agora IoT SDK](https://docs.agora.io/en/iot)
- [Conversational AI Agent API](https://docs.agora.io/en/conversational-ai)
- [ESP32-S3-Korvo-2 Hardware Guide](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)
