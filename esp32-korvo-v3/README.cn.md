# ESP32-S3-Korvo-2 V3 上的 Agora 对话式 AI 代理

*[English](./README.md) | 简体中文*

## 概述

这是一个运行在 **ESP32-S3-Korvo-2 V3** 开发板上的基于 ESP32-S3 的实时语音对话客户端。该项目通过 **Agora RTC** 平台和 **Agora 对话式 AI 代理 API v2** 实现与 AI 代理的自然语音对话。

开发板加入 RTC 频道，与由 Agora 对话式 AI 服务管理的 AI 代理进行通信。AI 代理提供语音识别（ASR）、大语言模型（LLM）处理和文本转语音（TTS）功能 - 所有这些都通过 ESP32 发起的简单 RESTful API 调用来控制。

### 主要特性

- ✅ **实时语音对话** - 通过 Agora RTC 实现低延迟音频流传输
- ✅ **简单的 AI 代理控制** - 通过 RESTful API 调用启动/停止代理
- ✅ **板载按钮控制** - 物理按钮（SET、MUTE、VOL+、VOL-）方便操作
- ✅ **回声消除（AEC）** - 内置回声消除提升音频质量
- ✅ **G.711 μ-law 音频** - 嵌入式系统高效音频编解码
- ✅ **可配置 AI 后端** - 支持 OpenAI、Azure 等多种 LLM 提供商
- ✅ **无需服务器** - ESP32 直接调用 Agora 对话式 AI API

### 硬件平台

本项目专为以下硬件设计：
- **开发板**: ESP32-S3-Korvo-2 V3
- **微控制器**: ESP32-S3 配 8MB PSRAM
- **音频编解码器**: ES8311 编解码器配 ES7210 ADC
- **麦克风**: 双板载麦克风

## 目录

- [工作原理](#工作原理)
- [硬件要求](#硬件要求)
- [文件结构](#文件结构)
- [环境配置](#环境配置)
- [项目配置](#项目配置)
- [编译和烧录](#编译和烧录)
- [使用指南](#使用指南)
- [项目架构](#项目架构)
- [Agora 对话式 AI API 参考](#agora-对话式-ai-api-参考)
- [关于 Agora](#关于-agora)

---

## 工作原理

本项目使用 **Agora 对话式 AI 代理 API v2** 通过简单的 RESTful API 调用来管理 AI 代理。以下是完整的工作流程：

### 系统架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ESP32-S3-Korvo-2 V3 开发板                               │
│                                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────────────────┐    │
│  │     按钮     │───►│  llm_main.c  │───►│     ai_agent.c           │    │
│  │ SET/MUTE/VOL │    │              │    │  (HTTP 客户端)           │    │
│  └──────────────┘    └──────────────┘    │                          │    │
│                                           │  - POST /join (启动)     │    │
│                                           │  - POST /leave (停止)    │    │
│                                           └──────────┬───────────────┘    │
│                                                      │ HTTPS                │
│  ┌──────────────┐    ┌──────────────┐              │                      │
│  │   麦克风     │───►│ audio_proc.c │              │                      │
│  │  (ES7210)    │    │              │              │                      │
│  └──────────────┘    └──────┬───────┘              │                      │
│                              │                      │                      │
│  ┌──────────────┐    ┌──────▼───────┐              │                      │
│  │   扬声器     │◄───│  rtc_proc.c  │              │                      │
│  │   (ES8311)   │    │              │              │                      │
│  └──────────────┘    └──────┬───────┘              │                      │
│                              │ RTC 音频 (G.711)     │                      │
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
        │          │   (RTC 网络)       │  │  API 服务器      │    │
        │          │                    │  │                  │    │
        │          │  - 音频流          │  │  POST /join      │    │
        │          │    传输            │  │  POST /leave     │    │
        │          │  - 低延迟          │  │                  │    │
        │          └─────────┬──────────┘  └────────┬─────────┘    │
        │                    │                      │               │
        │                    │                      │               │
        │          ┌─────────▼──────────────────────▼─────────┐    │
        │          │  Agora 对话式 AI 代理服务               │    │
        │          │                                          │    │
        │          │  ┌────────────────────────────────────┐ │    │
        │          │  │   AI 代理（每个频道一个）          │ │    │
        │          │  │                                    │ │    │
        │          │  │   ┌─────────┐  ┌──────┐  ┌─────┐ │ │    │
        │          │  │   │   ASR   │─►│ LLM  │─►│ TTS │ │ │    │
        │          │  │   │(语音    │  │对话  │  │语音 │ │ │    │
        │          │  │   │ 转文字) │  │ GPT  │  │输出 │ │ │    │
        │          │  │   └─────────┘  └──────┘  └─────┘ │ │    │
        │          │  │                                    │ │    │
        │          │  │   从 /join API 获取的配置:        │ │    │
        │          │  │   - LLM: OpenAI/Azure/等          │ │    │
        │          │  │   - TTS: Microsoft/Cartesia/等    │ │    │
        │          │  │   - ASR: 语言设置                 │ │    │
        │          │  └────────────────────────────────────┘ │    │
        │          └───────────────────────────────────────────┘    │
        │                      Agora 云                             │
        └───────────────────────────────────────────────────────────┘
```

### 工作流程序列

```
ESP32 开发板        Agora API 服务器     Agora RTC 网络      AI 代理服务
     │                      │                       │                      │
     │  1. 用户按下         │                       │                      │
     │     SET 按钮         │                       │                      │
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
     │                      │  4. 使用 LLM/TTS/ASR 配置创建 AI 代理       │
     │                      │                       │                      │
     │                      │                       │◄─────────────────────│
     │                      │  5. 代理加入频道 "X" (UID: 1001)            │
     │                      │                       │                      │
     ├──────────────────────────────────────────────►│                      │
     │  6. 开发板加入频道 "X" (UID: 1002)           │                      │
     │                      │                       │                      │
     │◄─────────────────────────────────────────────┤                      │
     │  7. RTC 连接成功     │                       │                      │
     │                      │                       │                      │
     │══════════════════════════════════════════════╬══════════════════════│
     │  8. 用户说话         │                       │                      │
     ├──────────────────────────────────────────────►│─────────────────────►│
     │      音频流 (G.711, 8kHz)                    │   音频送至 ASR       │
     │                      │                       │                      │
     │                      │                       │        ┌─────────────┤
     │                      │                       │        │ 9. ASR→LLM  │
     │                      │                       │        │    处理     │
     │                      │                       │        └─────────────►│
     │                      │                       │                      │
     │◄─────────────────────────────────────────────┤◄─────────────────────│
     │  10. AI 响应音频（TTS 生成）                 │   LLM→TTS→音频       │
     │      通过扬声器播放                          │                      │
     │══════════════════════════════════════════════╬══════════════════════│
     │                      │                       │                      │
     │  11. 用户按下        │                       │                      │
     │      MUTE 按钮       │                       │                      │
     ├─────────────────────►│                       │                      │
     │  12. POST /leave     │                       │                      │
     │      {agent_id: "xx"}│                       │                      │
     │◄─────────────────────│                       │                      │
     │  13. HTTP 200 OK     │                       │                      │
     │                      │                       │                      │
     │                      ├──────────────────────────────────────────────►│
     │                      │  14. 停止并销毁 AI 代理                      │
     │                      │                       │                      │
     ├──────────────────────────────────────────────►│                      │
     │  15. 离开 RTC 频道                           │                      │
     │                      │                       │                      │
```

### 关键组件

1. **ESP32 应用程序** (`main/ai_agent.c`)
   - 向 Agora 对话式 AI API 发送 HTTP POST 请求
   - **启动代理**: `POST /projects/{appid}/agents/join` 附带 LLM/TTS/ASR 配置
   - **停止代理**: `POST /projects/{appid}/agents/leave` 附带代理 ID

2. **Agora IoT SDK** (`components/agora_iot_sdk/`)
   - 提供设备侧使用的嵌入式 Agora RTC SDK
   - 负责 ESP32 上的 RTC 入会/离会和实时音频传输

3. **Agora 对话式 AI 服务**（云端）
   - 接收来自 ESP32 的 RESTful API 调用
   - 为每个频道动态创建/销毁 AI 代理
   - 每个代理包括：ASR（语音转文字）→ LLM（对话）→ TTS（文字转语音）

4. **Agora RTC 网络**（云端）
   - 在 ESP32 开发板和 AI 代理之间传输实时音频
   - 使用 G.711 μ-law 编解码器，8kHz 采样率

---

## 硬件要求

### 所需组件

1. **ESP32-S3-Korvo-2 V3 开发板**
   - 产品: [Espressif ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)
   - 特性: 双麦克风、ES8311 编解码器、ES7210 ADC、LCD 显示屏

2. **扬声器**
   - 连接到开发板的扬声器输出接口
   - 推荐: 4Ω 3W 扬声器

3. **USB 线缆**
   - USB-C 线缆用于编程和供电

### 硬件接口

ESP32-S3-Korvo-2 V3 集成了：

| 接口 | 功能 | 说明 |
|-----------|----------|-------------|
| **I2C** | 编解码器控制 | ES8311、ES7210 控制 |
| **I2S** | 音频数据 | 高质量音频流传输 |
| **按钮** | 用户控制 | SET、MUTE、VOL+、VOL- 按钮 |
| **USB** | 编程和供电 | USB-C 连接 |

---

## 文件结构

```
esp32-korvo-v3-convo/
├── CMakeLists.txt
├── components/
│   └── agora_iot_sdk/                  Agora IoT SDK
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── agora_rtc_api.h
│       └── libs/                       预编译库
│           ├── libagora-cjson.a
│           ├── libahpl.a
│           └── librtsa.a
├── main/                               应用代码
│   ├── app_config.h                    内嵌的 AI Agent JSON 配置
│   ├── ai_agent.c                      AI Agent API 客户端 (HTTP POST /join, /leave)
│   ├── ai_agent.h
│   ├── audio_proc.c                    音频处理管道 (I2S, AEC)
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC 处理 (加入/离开频道)
│   ├── rtc_proc.h
│   ├── llm_main.c                      主应用程序 (WiFi, 按钮控制)
│   ├── common.h
│   ├── CMakeLists.txt
│   ├── project_config.h                本地凭据和 WiFi 配置
│   └── Kconfig.projbuild
├── partitions.csv                      Flash 分区表
├── sdkconfig.defaults                  ESP-IDF 默认配置
├── sdkconfig.defaults.esp32s3          ESP32-S3 专用配置
├── .gitignore
└── README.md
```

---

## 环境配置

### 前置要求

- **操作系统**: Linux (Ubuntu 20.04+)、macOS 或 Windows
- **ESP-IDF**: v5.2.3 (commitId: c9763f62dd00c887a1a8fafe388db868a7e44069)
- **ESP-ADF**: v2.7 (commitId: 9cf556de500019bb79f3bb84c821fda37668c052)
- **Python**: 3.8+

### 步骤 1: 安装 ESP-IDF v5.2.3

**Linux/macOS:**
```bash
# 创建 esp 目录
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF
git clone -b v5.2.3 --recursive https://github.com/espressif/esp-idf.git

# 安装依赖
cd esp-idf
./install.sh esp32s3

# 设置环境变量（添加到 ~/.bashrc 或 ~/.zshrc）
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

**Windows:**
- 从以下链接下载 IDF v5.2.3（离线安装程序）：[ESP-IDF Windows 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.3/esp32/get-started/windows-setup.html)
- 按照安装向导操作

### 步骤 2: 安装 ESP-ADF v2.7

**Linux/macOS:**
```bash
cd ~/esp

# 克隆 ESP-ADF
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# 设置环境变量
export ADF_PATH=~/esp/esp-adf
echo 'export ADF_PATH=~/esp/esp-adf' >> ~/.bashrc  # 或 ~/.zshrc
```

**Windows:**
```bash
# 将 ADF 下载到 Espressif/frameworks 目录
cd C:\Espressif\frameworks
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# 设置 ADF_PATH 环境变量
setx ADF_PATH C:\Espressif\frameworks\esp-adf
```

### 步骤 3: 应用 IDF 补丁

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

### 步骤 4: 下载 Agora IoT SDK

```bash
cd /path/to/esp32-korvo-v3-convo/components

# 下载并解压 Agora IoT SDK
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
mkdir -p agora_iot_sdk && tar -xf agora_iot_sdk.tar -C agora_iot_sdk
```

解压后，验证目录结构：
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

## 项目配置

这个项目现在使用 **两个配置文件**：

- `main/project_config.h`：固件本地编译时使用的配置，包括 Agora 凭据、RTC App ID/Token 和 Wi‑Fi。
- `main/app_config.h`：通过 `EMBED_TXTFILES` 嵌入固件的 JSON，请求 Agora Conversational AI Agent `/join` 接口时会直接作为配置请求体发送。

### 步骤 1: 编辑 `main/project_config.h`

先填写项目级本地宏配置：

```c
#pragma once

#define AGORA_AI_AGENT_API_URL "https://api.agora.io/api/conversational-ai-agent/v2/projects"
#define AGORA_API_KEY          "YOUR_AGORA_API_KEY"
#define AGORA_API_SECRET       "YOUR_AGORA_API_SECRET"

#define AGORA_APP_ID           "YOUR_AGORA_APP_ID"
#define AGORA_RTC_TOKEN        ""   // 可选：如果项目启用了 RTC token，这里填写

#define WIFI_SSID              "YOUR_WIFI_SSID"
#define WIFI_PASSWORD          "YOUR_WIFI_PASSWORD"
```

说明：

1. `AGORA_API_KEY` 和 `AGORA_API_SECRET` 用于调用 Conversational AI Agent REST API 时的 HTTP Basic Auth。
2. `AGORA_APP_ID` 和 `AGORA_RTC_TOKEN` 会在 `llm_main.c` 中写入 RTC 运行时配置。

### 步骤 2: 编辑 `main/app_config.h`

`main/app_config.h` 是 JSON 文档。编译时它会被嵌入固件，并作为 AI Agent 的配置请求体发送出去。

至少需要确认下面这些字段：

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

关键字段说明：

1. `name`：Agora agent 名称。
2. `properties.channel`：开发板与云端 agent 共用的 RTC 频道名。
3. `properties.agent_rtc_uid`：云端 AI agent 使用的 RTC UID。
4. `properties.remote_rtc_uids[0]`：开发板设备侧的 RTC UID，必须与设备实际作为远端目标使用的 UID 保持一致。
5. `properties.parameters.output_audio_codec`：固件解析时必填，目前支持 `PCMU` 和 `G722`。
6. `properties.llm` / `properties.asr` / `properties.tts`：启动 agent 时原样发送给 Agora 的模型与服务商参数。

**如何获取 Agora 凭据：**

1. 注册或登录 [Agora Console](https://console.agora.io/)
2. 创建新项目或选择现有项目
3. 从项目仪表板复制 **App ID**
4. 在项目设置中启用 **Conversational AI（对话式 AI）** 服务
5. 为 Conversational AI Agent API v2 生成 **API 凭据**（API Key 和 Secret）

详细说明请参考：[管理 Agora 账户 - 对话式 AI](https://docs.agora.io/cn/conversational-ai/get-started/manage-agora-account)

> **注意：** 在使用 REST API 之前，必须先在 Agora 项目中启用对话式 AI 功能。

---

## 编译和烧录

### 步骤 1: 设置环境

**Linux/macOS:**
```bash
# 加载 ESP-IDF 环境
get_idf

# 或者如果没有创建别名：
. $HOME/esp/esp-idf/export.sh

# 设置 ADF_PATH（如果不在 .bashrc/.zshrc 中）
export ADF_PATH=~/esp/esp-adf
```

**Windows:**
```bash
# 打开 ESP-IDF PowerShell 或 CMD
# ADF_PATH 应该已经在系统环境变量中设置
```

### 步骤 2: 导航到项目目录

```bash
cd /path/to/esp32-korvo-v3-convo
```

### 步骤 3: 配置目标芯片

```bash
idf.py set-target esp32s3
```

### 步骤 4: 检查可选项目设置

Wi‑Fi 现在在 `main/project_config.h` 中配置，因此不再需要用 `idf.py menuconfig` 填写 SSID 和密码。

### 步骤 5: 编译

```bash
idf.py build
```

**编译输出：**
您应该看到：
```
Project build complete. To flash, run:
  idf.py flash
```

### 步骤 6: 烧录到设备

**Linux/macOS:**
```bash
# 烧录并监控
idf.py -p /dev/ttyUSB0 flash monitor

# 或在 macOS 上（通常显示为）
idf.py -p /dev/cu.usbserial-* flash monitor
```

**Windows:**
```bash
idf.py -p COM3 flash monitor
```

---

## 使用指南

### 快速开始

1. **上电**
   - 通过 USB-C 连接 ESP32-S3-Korvo-2 V3
   - 设备将启动并连接到 WiFi
   - 等待消息：`Press [SET] key to Join the Ai Agent ...`

2. **开始对话**
   - 按一次 **SET 按钮**
   - 开发板向 Agora API 发送 `POST /join` 请求
   - AI 代理被创建并加入 RTC 频道
   - 开发板加入同一 RTC 频道
   - 听到问候语后开始说话！

3. **停止对话**
   - 按 **MUTE 按钮**
   - 开发板向 Agora API 发送 `POST /leave` 请求
   - AI 代理被销毁
   - 开发板离开 RTC 频道

4. **音量控制**
   - 按 **VOL+** 按钮增加音量（每次增加 10，最大 100）
   - 按 **VOL-** 按钮降低音量（每次减少 10，最小 0）

### 串口监视器输出

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

## 项目架构

### 音频处理管道

```
麦克风（ES7210 ADC）
    ↓ I2S
I2S 流读取器
    ↓
算法流（AEC）
    ↓
原始流
    ↓
RTC 编码器（G.711 μ-law）
    ↓
Agora RTC → AI 代理（云端）
    ↓
    ASR（语音转文字）
    ↓
    LLM（对话处理）
    ↓
    TTS（文字转语音）
    ↓
Agora RTC ← 音频响应
    ↓
RTC 解码器
    ↓
原始流
    ↓
I2S 流写入器
    ↓ I2S
扬声器（ES8311 编解码器）
```

## Agora 对话式 AI API 参考

### API 文档

- [Agora 对话式 AI - Join API](https://docs.agora.io/cn/conversational-ai/rest-api/agent/join)
- [Agora 对话式 AI - Leave API](https://docs.agora.io/cn/conversational-ai/rest-api/agent/leave)

---

## 关于 Agora

Agora 提供实时互动 API，为全球数十亿分钟的实时语音、视频和互动流媒体体验提供支持。Agora IoT SDK 和对话式 AI 服务将这些功能引入 ESP32 等嵌入式设备。

### 资源

- [Agora Console](https://console.agora.io/)
- [Agora 文档](https://docs.agora.io/cn/)
- [Agora IoT SDK](https://docs.agora.io/cn/iot)
- [Conversational AI Agent API](https://docs.agora.io/cn/conversational-ai)
- [ESP32-S3-Korvo-2 硬件指南](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)

