# Agent
- CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1
- 尽量优先使用Agent Teams模式来进行多智能体开发，并且使用tmux模式

# Project
## 环境变量
- 下述所需的环境变量来自.env，由${}包裹
- 环境变量只是给编程环境Claude使用，ESP32需要按实际进行编码

## ESP-IDF 环境
- 通过 `C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1` 激活环境配置
- 优先使用idf.py命令进行ESP-IDF相关操作
- **ESP-IDF 命令必须使用 `PowerShell` 工具**，不能用 Bash（IDF 环境在 PowerShell 下才配置完整）
- "IDF_TOOLS_PATH" = "C:\Espressif\tools"
- "IDF_COMPONENT_LOCAL_STORAGE_URL" = "file://C:\Espressif\tools"
- "IDF_PATH" = "C:\esp\v6.0\esp-idf"
- "ESP_ROM_ELF_DIR" = "C:\Espressif\tools\esp-rom-elfs\20241011"
- "OPENOCD_SCRIPTS" = "C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\openocd-esp32\share\openocd\scripts"
- "IDF_PYTHON_ENV_PATH" = "C:\Espressif\tools\python\v6.0\venv"

- ESP-IDF 文档：`C:\esp\v6.0\esp-idf\docs\zh_CN`
- COM 口自动检测：**优先读取最新插入的 COM 口，默认排序最后的**，不要硬编码

## ESP32
- 型号：ESP32-WROVER-DEV
- 摄像头型号：OV2640
- GPIO13 PWM 连接了舵机(SG90)
- WiFi_SSID=${WiFi_SSID}
- WIFI_PASS=${WIFI_PASS}
- ESP32的网络配置
  - IP：10.0.0.110
  - ROUTE: 10.0.0.2
  - DNS: 10.0.0.2

## ESP32调试与测试
- 需要通过串口日志进行闭环验证，如果报错Monitor requires TTY，则通过抓取最近串口日志来调试
- 需要通过pytest编写关于接口的集成测试
  
## 本地环境
- windows主机
- WSL2 Ubuntu带Dokcer环境

## 远程服务器
- 无需密码
- REMOTE_SERVER=${REMOTE_SERVER}
- REMOTE_SERVER_SSH_USER=${REMOTE_SERVER_SSH_USER}
- REMOTE_SERVER_SSH_PORT=${REMOTE_SERVER_SSH_PORT}
- 使用Docker部署，需在本地验证通过再部署远程服务器
- 远程服务域名为${REMOTE_DOMAIN}
- 远程暴露端口范围${REMOTE_PORT_RANGE}


# graphify
- **graphify** (`~/.claude/skills/graphify/SKILL.md`) - any input to knowledge graph. Trigger: `/graphify`
When the user types `/graphify`, invoke the Skill tool with `skill: "graphify"` before doing anything else.

## graphify

This project has a graphify knowledge graph at graphify-out/.

Rules:
- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- If graphify-out/wiki/index.md exists, navigate it instead of reading raw files
- After modifying code files in this session, run `graphify update .` to keep the graph current (AST-only, no API cost)
