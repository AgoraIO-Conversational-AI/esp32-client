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
- [Troubleshooting](#troubleshooting)

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

2. **Agora Conversational AI Service** (Cloud)
   - Receives RESTful API calls from ESP32
   - Creates/destroys AI agents dynamically per channel
   - Each agent includes: ASR (speech-to-text) → LLM (conversation) → TTS (text-to-speech)

3. **Agora RTC Network** (Cloud)
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
│   ├── app_config.h.example            ← Configuration template
│   ├── ai_agent.c                      AI Agent API client (HTTP POST /join, /leave)
│   ├── ai_agent.h
│   ├── audio_proc.c                    Audio pipeline (I2S, AEC)
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC handling (join/leave channel)
│   ├── rtc_proc.h
│   ├── llm_main.c                      Main application (WiFi, button control)
│   ├── common.h
│   ├── CMakeLists.txt
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
tar -xvf agora_iot_sdk.tar
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

### Step 1: Create Configuration File

```bash
# Copy the example configuration
cp main/app_config.h.example main/app_config.h
```

### Step 2: Edit `main/app_config.h`

Open `main/app_config.h` and configure the following:

#### 1. Agora Credentials

```c
// Agora Conversational AI Agent API
#define AGORA_API_KEY                "YOUR_AGORA_API_KEY"
#define AGORA_API_SECRET             "YOUR_AGORA_API_SECRET"

// Agora RTC Configuration
#define AGORA_APP_ID                 "YOUR_AGORA_APP_ID"
#define AGORA_RTC_TOKEN              ""  // Optional: Use token for production
#define BOARD_RTC_UID                1002  // Board's UID for RTC connection
```

**How to get your Agora credentials:**

1. Sign up or log in to [Agora Console](https://console.agora.io/)
2. Create a new project or select an existing project
3. Copy your **App ID** from the project dashboard
4. Enable **Conversational AI** service in your project settings
5. Generate **API credentials** (API Key and Secret) for Conversational AI Agent API v2

For detailed instructions, see: [Manage Agora Account - Conversational AI](https://docs.agora.io/en/conversational-ai/get-started/manage-agora-account)

> **Note:** The Conversational AI feature must be enabled in your Agora project before you can use the RESTful API.

#### 2. AI Agent Configuration

```c
// AI Agent Configuration
#define AI_AGENT_NAME                "my_agent"
#define AI_AGENT_CHANNEL_NAME        "test_channel"
#define AI_AGENT_TOKEN               ""  // Optional: RTC token for agent
#define AI_AGENT_RTC_UID             1001  // AI Agent's UID in RTC channel
#define AI_AGENT_USER_ID             1002  // Board's UID (remote_rtc_uid)
```

#### 3. LLM Configuration

**OpenAI Example:**
```c
#define LLM_API_URL                  "https://api.openai.com/v1/chat/completions"
#define LLM_API_KEY                  "YOUR_OPENAI_API_KEY"
#define LLM_MODEL                    "gpt-4o-mini"
#define LLM_SYSTEM_MESSAGE           "You are a helpful chatbot."
#define LLM_GREETING                 "Hello, how can I assist you today?"
```

**Azure OpenAI Example:**
```c
#define LLM_API_URL                  "https://your-resource.openai.azure.com/openai/deployments/your-deployment/chat/completions?api-version=2024-02-15-preview"
#define LLM_API_KEY                  "your-azure-api-key"
#define LLM_MODEL                    "gpt-4"
```

#### 4. TTS Configuration

**Microsoft Azure TTS:**
```c
#define TTS_VENDOR                   "microsoft"
#define TTS_API_KEY                  "YOUR_AZURE_TTS_API_KEY"
#define TTS_REGION                   "YOUR_AZURE_REGION"  // e.g., "japanwest", "eastus"
#define TTS_VOICE_NAME               "en-US-AndrewMultilingualNeural"
```

For other TTS providers (Cartesia, ElevenLabs, etc.), see: [Agora TTS Overview](https://docs.agora.io/en/conversational-ai/models/tts/overview)

#### 5. ASR Configuration

```c
#define ASR_LANGUAGE                 "en-US"
```

#### 6. WiFi Configuration (via menuconfig)

WiFi credentials are configured through `idf.py menuconfig`:

```bash
idf.py menuconfig
```

Navigate to: `Agora Demo for ESP32` → Configure WiFi SSID and Password

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

### Step 4: Configure WiFi

```bash
idf.py menuconfig
```

Navigate to: `Agora Demo for ESP32` → Configure your WiFi SSID and Password

**Optional FreeRTOS Configuration:**
- Navigate to: `Component config` → `FreeRTOS` → `Kernel`
- Enable `configENABLE_BACKWARD_COMPATIBILITY`

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

**Fix Permission Issues (Linux):**
```bash
sudo usermod -aG dialout $USER
# Logout and login again
```

**Troubleshooting Flash Issues:**

If you encounter connection errors at high baud rates:
```bash
# Try lower baud rate
idf.py -p /dev/ttyUSB0 -b 115200 flash monitor
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

### Component Overview

| Component | Purpose |
|-----------|---------|
| **llm_main.c** | Main application, WiFi init, button monitoring |
| **ai_agent.c** | Agora Conversational AI API client (POST /join, POST /leave) |
| **audio_proc.c** | Audio pipeline setup (I2S, AEC, streams) |
| **rtc_proc.c** | Agora RTC handling (join/leave channel, audio TX/RX) |
| **common.h** | Shared definitions and structures |
| **app_config.h** | Configuration parameters (credentials, settings) |

### API Implementation Details

The `ai_agent.c` file implements two main functions:

#### `ai_agent_start()`
- Sends `POST /projects/{appid}/agents/join` request
- Includes LLM, TTS, ASR configuration in JSON body
- Uses HTTP basic authentication (API Key:Secret)
- Returns agent ID on success (HTTP 200)

#### `ai_agent_stop()`
- Sends `POST /projects/{appid}/agents/leave` request
- Includes agent ID in JSON body
- Destroys the AI agent in Agora cloud

---

## Agora Conversational AI API Reference

This project uses **Agora Conversational AI Agent API v2** via RESTful HTTP calls.

### Start Agent Request

**Endpoint:** `POST https://api.agora.io/api/conversational-ai-agent/v2/projects/{appid}/agents/join`

**Authentication:** HTTP Basic Auth (API Key:API Secret in Base64)

**Request Body:**
```json
{
  "name": "my_agent",
  "properties": {
    "channel": "test_channel",
    "token": "",
    "agent_rtc_uid": 1001,
    "remote_rtc_uids": [1002],
    "parameters": {
      "output_audio_codec": "PCMU"
    },
    "idle_timeout": 120,
    "advanced_features": {
      "enable_aivad": true
    },
    "llm": {
      "api_url": "https://api.openai.com/v1/chat/completions",
      "api_key": "YOUR_KEY",
      "model": "gpt-4o-mini",
      "system_message": "You are a helpful chatbot.",
      "greeting": "Hello, how can I assist you today?"
    },
    "tts": {
      "vendor": "microsoft",
      "api_key": "YOUR_KEY",
      "region": "japanwest",
      "voice_name": "en-US-AndrewMultilingualNeural"
    },
    "asr": {
      "language": "en-US"
    }
  }
}
```

**Response (HTTP 200):**
```json
{
  "code": "ok",
  "data": {
    "agent_id": "abc123..."
  }
}
```

### Stop Agent Request

**Endpoint:** `POST https://api.agora.io/api/conversational-ai-agent/v2/projects/{appid}/agents/leave`

**Request Body:**
```json
{
  "agent_id": "abc123..."
}
```

**Response (HTTP 200):**
```json
{
  "code": "ok"
}
```

### API Documentation

For complete API documentation:
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

---

## License

This project includes:
- Agora IoT SDK (proprietary)
- ESP-IDF (Apache 2.0)
- ESP-ADF (Apache 2.0)

See individual component licenses for details.

---

## Contributing

Contributions are welcome! Please submit pull requests or open issues for bugs and feature requests.

---

**For technical support, visit the [Agora Developer Community](https://www.agora.io/en/community/) or contact Agora support.**
