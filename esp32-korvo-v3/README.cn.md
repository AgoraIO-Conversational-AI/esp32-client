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
- [故障排除](#故障排除)

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

2. **Agora 对话式 AI 服务**（云端）
   - 接收来自 ESP32 的 RESTful API 调用
   - 为每个频道动态创建/销毁 AI 代理
   - 每个代理包括：ASR（语音转文字）→ LLM（对话）→ TTS（文字转语音）

3. **Agora RTC 网络**（云端）
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
│   ├── app_config.h.example            ← 配置模板
│   ├── ai_agent.c                      AI Agent API 客户端 (HTTP POST /join, /leave)
│   ├── ai_agent.h
│   ├── audio_proc.c                    音频处理管道 (I2S, AEC)
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC 处理 (加入/离开频道)
│   ├── rtc_proc.h
│   ├── llm_main.c                      主应用程序 (WiFi, 按钮控制)
│   ├── common.h
│   ├── CMakeLists.txt
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
tar -xvf agora_iot_sdk.tar
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

### 步骤 1: 创建配置文件

```bash
# 复制示例配置文件
cp main/app_config.h.example main/app_config.h
```

### 步骤 2: 编辑 `main/app_config.h`

打开 `main/app_config.h` 并配置以下内容：

#### 1. Agora 凭据

```c
// Agora 对话式 AI 代理 API
#define AGORA_API_KEY                "YOUR_AGORA_API_KEY"
#define AGORA_API_SECRET             "YOUR_AGORA_API_SECRET"

// Agora RTC 配置
#define AGORA_APP_ID                 "YOUR_AGORA_APP_ID"
#define AGORA_RTC_TOKEN              ""  // 可选：生产环境使用 token
#define BOARD_RTC_UID                1002  // 开发板的 RTC 连接 UID
```

**如何获取 Agora 凭据：**

1. 注册或登录 [Agora Console](https://console.agora.io/)
2. 创建新项目或选择现有项目
3. 从项目仪表板复制 **App ID**
4. 在项目设置中启用 **Conversational AI（对话式 AI）** 服务
5. 为 Conversational AI Agent API v2 生成 **API 凭据**（API Key 和 Secret）

详细说明请参考：[管理 Agora 账户 - 对话式 AI](https://docs.agora.io/cn/conversational-ai/get-started/manage-agora-account)

> **注意：** 在使用 RESTful API 之前，必须在 Agora 项目中启用对话式 AI 功能。

#### 2. AI 代理配置

```c
// AI 代理配置
#define AI_AGENT_NAME                "my_agent"
#define AI_AGENT_CHANNEL_NAME        "test_channel"
#define AI_AGENT_TOKEN               ""  // 可选：代理的 RTC token
#define AI_AGENT_RTC_UID             1001  // AI 代理在 RTC 频道中的 UID
#define AI_AGENT_USER_ID             1002  // 开发板的 UID (remote_rtc_uid)
```

#### 3. LLM 配置

**OpenAI 示例：**
```c
#define LLM_API_URL                  "https://api.openai.com/v1/chat/completions"
#define LLM_API_KEY                  "YOUR_OPENAI_API_KEY"
#define LLM_MODEL                    "gpt-4o-mini"
#define LLM_SYSTEM_MESSAGE           "You are a helpful chatbot."
#define LLM_GREETING                 "Hello, how can I assist you today?"
```

**Azure OpenAI 示例：**
```c
#define LLM_API_URL                  "https://your-resource.openai.azure.com/openai/deployments/your-deployment/chat/completions?api-version=2024-02-15-preview"
#define LLM_API_KEY                  "your-azure-api-key"
#define LLM_MODEL                    "gpt-4"
```

#### 4. TTS 配置

**Microsoft Azure TTS：**
```c
#define TTS_VENDOR                   "microsoft"
#define TTS_API_KEY                  "YOUR_AZURE_TTS_API_KEY"
#define TTS_REGION                   "YOUR_AZURE_REGION"  // 例如："japanwest"、"eastus"
#define TTS_VOICE_NAME               "en-US-AndrewMultilingualNeural"
```

有关其他 TTS 提供商（Cartesia、ElevenLabs 等），请参阅：[Agora TTS 概览](https://docs.agora.io/cn/conversational-ai/models/tts/overview)

#### 5. ASR 配置

```c
#define ASR_LANGUAGE                 "en-US"
```

#### 6. WiFi 配置（通过 menuconfig）

WiFi 凭据通过 `idf.py menuconfig` 配置：

```bash
idf.py menuconfig
```

导航到：`Agora Demo for ESP32` → 配置 WiFi SSID 和密码

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

### 步骤 4: 配置 WiFi

```bash
idf.py menuconfig
```

导航到：`Agora Demo for ESP32` → 配置您的 WiFi SSID 和密码

**可选的 FreeRTOS 配置：**
- 导航到：`Component config` → `FreeRTOS` → `Kernel`
- 启用 `configENABLE_BACKWARD_COMPATIBILITY`

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

**修复权限问题（Linux）：**
```bash
sudo usermod -aG dialout $USER
# 登出并重新登录
```

**烧录问题故障排除：**

如果在高波特率下遇到连接错误：
```bash
# 尝试较低的波特率
idf.py -p /dev/ttyUSB0 -b 115200 flash monitor
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

### 按钮功能

| 按钮 | 功能 | API 调用 |
|--------|----------|----------|
| **SET** | 启动 AI 代理 | POST /join |
| **MUTE** | 停止 AI 代理 | POST /leave |
| **VOL+** | 增加音量 | - |
| **VOL-** | 降低音量 | - |

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

### 组件概览

| 组件 | 用途 |
|-----------|---------|
| **llm_main.c** | 主应用程序、WiFi 初始化、按钮监控 |
| **ai_agent.c** | Agora 对话式 AI API 客户端（POST /join、POST /leave）|
| **audio_proc.c** | 音频管道设置（I2S、AEC、流）|
| **rtc_proc.c** | Agora RTC 处理（加入/离开频道、音频 TX/RX）|
| **common.h** | 共享定义和结构 |
| **app_config.h** | 配置参数（凭据、设置）|

### API 实现细节

`ai_agent.c` 文件实现了两个主要函数：

#### `ai_agent_start()`
- 发送 `POST /projects/{appid}/agents/join` 请求
- JSON 请求体中包含 LLM、TTS、ASR 配置
- 使用 HTTP 基本身份验证（API Key:Secret）
- 成功时返回代理 ID（HTTP 200）

#### `ai_agent_stop()`
- 发送 `POST /projects/{appid}/agents/leave` 请求
- JSON 请求体中包含代理 ID
- 在 Agora 云端销毁 AI 代理

---

## Agora 对话式 AI API 参考

本项目通过 RESTful HTTP 调用使用 **Agora 对话式 AI 代理 API v2**。

### 启动代理请求

**端点:** `POST https://api.agora.io/api/conversational-ai-agent/v2/projects/{appid}/agents/join`

**身份验证:** HTTP 基本身份验证（Base64 编码的 API Key:API Secret）

**请求体:**
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

**响应（HTTP 200）:**
```json
{
  "code": "ok",
  "data": {
    "agent_id": "abc123..."
  }
}
```

### 停止代理请求

**端点:** `POST https://api.agora.io/api/conversational-ai-agent/v2/projects/{appid}/agents/leave`

**请求体:**
```json
{
  "agent_id": "abc123..."
}
```

**响应（HTTP 200）:**
```json
{
  "code": "ok"
}
```

### API 文档

完整的 API 文档：
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

---

## 许可证

本项目包含：
- Agora IoT SDK（专有）
- ESP-IDF（Apache 2.0）
- ESP-ADF（Apache 2.0）

详情请参阅各组件的许可证。

---

## 贡献

欢迎贡献！请提交 pull request 或针对 bug 和功能请求提出 issue。

---

**技术支持请访问 [Agora 开发者社区](https://www.agora.io/cn/community/) 或联系 Agora 支持团队。**
