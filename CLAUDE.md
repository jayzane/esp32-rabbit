# Agent
- CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1
- 尽量优先使用Agent Teams模式来进行多智能体开发，并且使用tmux模式

# Project
## 环境变量
下述所需的环境变量来自.env，由${}包裹

## ESP-IDF 环境
- 通过 `C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1` 激活环境配置
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
- GPIO13 PWM 连接了舵机
- WiFi_SSID=${WiFi_SSID}
- WIFI_PASS=${WIFI_PASS}
- ESP32_IP=10.0.0.110
  
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