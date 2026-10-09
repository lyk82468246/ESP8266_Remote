# ESP8266 WiFi 外设下位机

对接 [Android_WIFI 协议 v1](https://github.com/zxu-source/Android_WIFI/blob/main/docs/protocol/wifi-control-v1.md)，仅查看了其协议与设计文档，没有克隆 Android 仓库。首版采用 Arduino ESP8266 Core，注册实际二态输出；默认 NodeMCU/ESP-12E 板载 LED（D4 = GPIO2，低电平亮）。板型尚未实物核验，此配置已获用户确认作为开发基线。

## 构建与运行

PlatformIO Core 6.1.18，平台 `espressif8266@4.2.1`（Arduino Core 3.1.2），ArduinoJson 6.21.5，开发板 `nodemcuv2`。

```powershell
pio run
pio run --target upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

COM3 换成自己的端口。也可在 Arduino IDE 打开 `ESP8266_Remote/ESP8266_Remote.ino`，安装 ESP8266 Core 3.1.2 和 ArduinoJson 6.21.5，选择 NodeMCU 1.0 (ESP-12E Module)。本工程源码使用 UTF-8。

当前工作区已在忽略目录 `.tools` / `.pio-core` 安装工具，不依赖系统的 `pio` 命令，可直接执行 `./tools/pio.ps1 run`；上面的上传/监视命令也可将 `pio` 替换为 `./tools/pio.ps1`。新机器运行 `python -m pip install --target .tools/python platformio==6.1.18` 后即可使用该入口。`nodemcuv2` 为默认 AP 构建；`run -e nodemcuv2_sta` 用于检查 STA 分支，真实 STA 凭据仍需在本地配置文件填写。

默认热点 `ESP8266-Remote-<芯片ID>`，演示密码 `esp8266-demo`，HTTP 端口 80，默认地址 `192.168.4.1`，以串口日志为准。手机系统设置连接热点后，Android 上位机填写该地址。首次连接先 GET device，再 GET state。热点不提供互联网。

要修改密码或使用现有路由器，将 `config.local.h.example` 复制为同目录下 `config.local.h`，修改配置；STA 设置 `REMOTE_USE_STA=1`，填写 SSID/密码。此文件被 Git 忽略。STA 不启用额外热点，连接过程不阻塞控制主循环，实际 IP 从串口读取。STA 连接中断会自动重连。

## API

| 请求 | 行为 |
| --- | --- |
| `GET /api/v1/device` | 设备身份、启动会话、固件版本和实际注册的能力 |
| `GET /api/v1/state` | 完整逻辑输出状态、quality、全局 revision |
| `PUT /api/v1/resources/led.main/properties/on` | 设置明确 boolean 目标状态 |

写入示例（bootId 替换为 GET device 返回值；每次操作使用新的 UUID）：

```json
{"requestId":"b294c92a-02e2-4b4f-b44e-254908bdcbf3","expectedBootId":"boot-当前启动会话","value":true}
```

设备 ID 来源于芯片 ID；bootId 每次启动由随机数生成，revision 从 0 开始，只有逻辑输出发生变化时增加。同值写入不会翻转。启动先关闭输出，断联保持当前值，重启不恢复上次值。成功状态是 GPIO 输出回读确认的逻辑状态，并非灯光或继电器触点反馈。驱动回读失败返回 DRIVER_ERROR 并尝试还原先前电平；物理硬件故障不能保证还原成功。

请求先完成校验再操作 GPIO；类型错误、旧 bootId、未知资源、无效 JSON 等拒绝请求不改变输出。支持协议规定的 400/404/405/409/413/415/422/500 错误及 JSON 格式，405 带 Allow。所有响应为 JSON 且禁止缓存。

HTTP 使用固定缓冲区、分段收发，最多一个活动连接；每轮最多处理 256 字节。请求体最多 1024 字节，在读取 body 前检查长度；本版响应最多 4 KiB（低于协议的 16 KiB 上限，超限明确报错）；头部最多 2048 字节，单行最多 255 字节。空闲接收超时 1.5 秒，整次接收超时 5 秒，未完整请求不会执行命令。要求 Content-Length 分帧，不支持 chunked、Expect: 100-continue 和 HTTP 长连接，主动返回错误。常规 Android HTTP JSON 请求需发送已知长度 body。

## 外设扩展

在 `Config.h` 的 `kOutputs` 增加实际连接的 LED/继电器，配置稳定 ID、显示名、GPIO 编号、有效电平。能力描述、路由和状态均由同一注册表生成，最多 8 路，检查重复 ID、重复 GPIO 和 flash 专用引脚。GPIO 号码不是 Dx 标签；GPIO0/2/15 有启动电平要求，GPIO1/3 已保留给串口，不能注册。更换开发板时核对原理图和板载 LED 接法。

首版仅实现 boolean 输出，不声明不存在的 PWM/传感器。后续数值和只读驱动需同时加入属性描述、类型/范围/步长校验及状态质量管理。LED 板载电阻无需额外接线；外接 LED 需要限流电阻，继电器需要适配 3.3 V 的驱动模块，GPIO 不直接驱动负载。

## 开发约定

从 [AGENTS.md](AGENTS.md) 进入，按任务阅读 [.agents](.agents/README.md) 中的固件、协议、验证规则和开发流程。`.gitignore` 排除构建缓存、下载工具链、Python 环境和真实网络配置；脱敏验收证据可提交，原始私密记录放在 `docs/testing/evidence/local/`。

## 验证

本机契约测试编译并执行同一 API/HTTP/驱动代码，使用 GPIO 替身覆盖幂等性、bootId、无效请求无副作用、长度边界、严格 JSON 和驱动失败。它不证明无线网络或真实引脚行为。

```powershell
# 首次通过 PlatformIO 安装 Windows 测试编译器：
pio pkg install --global --tool platformio/toolchain-gccmingw32@1.50100.0
./tools/test_native.ps1 -Compiler '你的g++.exe绝对路径'
# 烧录后连接板端热点，执行真实设备契约检查：
python tools/check_device.py --host http://192.168.4.1 --cycles 10
```

实物脚本会实际开关 LED：10 轮对应 20 次状态变化，额外检查重复写、类型、旧会话、JSON、媒体类型和超限请求，正常结束输出关闭。测试期间保持单客户端，人工观察 LED；脚本不自动烧录。未连接真实板前，不宣称实物或 Android 真机验收通过。
