# ESP8266 WiFi 外设下位机

对接 [Android_WIFI 协议 v1](https://github.com/zxu-source/Android_WIFI/blob/main/docs/protocol/wifi-control-v1.md)及[功能清单规范](https://github.com/zxu-source/Android_WIFI/blob/main/docs/protocol/capability-list.md)，当前固件版本 0.4.0，适配依据为上位机提交 `320f9c0`（2026-10-16）。仅读取相关文件，没有克隆 Android 仓库。用户提供板卡标识为 LOLIN / wemos.cc / NEW NodeMCU V3；沿用 ESP-12E 构建基线，按用户要求以 3.3 V 作为电位器电压换算基准，尚未实物校准 A0。当前支持板载 LED、两路外接开关 LED、两路 PWM LED 和 A0 电位器。**接线表、电阻与 ADC 分压方案见 [外设接线说明](docs/hardware/peripherals.md)。**

## 构建与运行

PlatformIO Core 6.1.18，平台 `espressif8266@4.2.1`（Arduino Core 3.1.2），ArduinoJson 6.21.5，开发板 `nodemcuv2`。

默认以 STA 模式加入 **2.4 GHz 的 SmartLife WiFi**，可以是手机热点或普通 WLAN。网络名称、密码和模式统一固定在受版本管理的 [WiFiConfig.h](ESP8266_Remote/WiFiConfig.h)，按用户要求随项目保存，构建前无需额外本地配置。更换网络时修改该文件并重新构建烧录。SSID 为空时仍会关闭 WiFi、停用 API 并保持输出关闭；不自动回退热点，也不额外写入 SDK 的持久配置。

```powershell
pio run
pio run --target upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

COM3 换成自己的端口。也可在 Arduino IDE 打开 `ESP8266_Remote/ESP8266_Remote.ino`，安装 ESP8266 Core 3.1.2 和 ArduinoJson 6.21.5，选择 NodeMCU 1.0 (ESP-12E Module)。本工程源码使用 UTF-8。

当前工作区已在忽略目录 `.tools` / `.pio-core` 安装工具，不依赖系统的 `pio` 命令，可直接执行 `./tools/pio.ps1 run`；上面的上传/监视命令也可将 `pio` 替换为 `./tools/pio.ps1`。新机器运行 `python -m pip install --target .tools/python platformio==6.1.18` 后即可使用该入口。

| 环境 | 模式 | 用途 |
| --- | --- | --- |
| `nodemcuv2` | 默认 STA，使用 WiFiConfig.h 的项目配置 | 常规构建/上传入口 |
| `nodemcuv2_sta` | 显式 STA | 保留既有 STA 构建入口 |
| `nodemcuv2_ap` | 显式 SoftAP | 不依赖手机热点凭据的板端热点演示 |

STA 的 IP 由 DHCP 分配，不固定为 `192.168.4.1`；以串口 `API: http://...` 为准。手机开热点时 App 选择“系统路由”，也可发现并选择设备。首次连接先 GET device，再 GET state。STA 连接和重连不阻塞主循环，断联保持输出。

需要开发板开热点时使用 `pio run -e nodemcuv2_ap`，上传也指定同一环境（`pio run -e nodemcuv2_ap --target upload --upload-port COM3`）。热点名 `ESP8266-Remote-<芯片ID>`，公开演示密码 `esp8266-demo`，可在 `WiFiConfig.h` 修改。手机手动连接板端热点后访问串口打印的 IP，通常为 `192.168.4.1:80`；该热点不提供互联网。

两种模式均广播 `_wifi-control._tcp.local.` 服务，实例名 `WiFi-Control-<deviceId>`，TXT 提供 `protocolVersion=1` 和 `deviceId`，SRV 端口 80。主循环持续服务 mDNS；断线、重连或 IP 变化后重新注册，注册失败每 5 秒重试，手动 IP 控制仍可使用。厂商热点的客户端隔离/组播限制可能影响可达性或发现，实际效果需真机验证。

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

设备 ID 来源于芯片 ID；bootId 每次启动由随机数生成，revision 从 0 开始，输出或采样值/可用性变化时增加。同值写入/采样不增加。启动先关闭输出，灯光断联保持当前值，重启不恢复上次值。二态输出由 GPIO 回读确认，PWM 回报是波形发生器接受的目标占空比；不等同于实际亮度或负载反馈。驱动失败返回 DRIVER_ERROR。

device 和 state 都提供 `capabilitiesRevision: 2`，电位器新增 voltage。当前注册表在一次启动内固定，名称或资源配置需重新构建并重启，bootId 随之改变；普通输出/采样变化不改变清单版本。当前不提供运行时热插拔；以后新增在线清单更新时，必须同时递增 capabilitiesRevision 和状态 revision，并原子提供匹配的描述/状态。

请求先完成校验再操作 GPIO；类型错误、旧 bootId、未知资源、无效 JSON 等拒绝请求不改变输出。支持协议规定的 400/404/405/409/413/415/422/500 错误及 JSON 格式，405 带 Allow。所有响应为 JSON 且禁止缓存。

HTTP 使用固定缓冲区、分段收发，最多一个活动连接；每轮最多处理 256 字节。请求体最多 1024 字节，在读取 body 前检查长度；本版响应最多 4 KiB（低于协议的 16 KiB 上限，超限明确报错）；头部最多 2048 字节，单行最多 255 字节。空闲接收超时 1.5 秒，整次接收超时 5 秒，未完整请求不会执行命令。要求 Content-Length 分帧，不支持 chunked、Expect: 100-continue 和 HTTP 长连接，主动返回错误。常规 Android HTTP JSON 请求需发送已知长度 body。

## 外设扩展

在 `Config.h` 的 `kResources` 配置实际外设，填写资源/属性 ID、名称、类型、GPIO、有效电平和数值约束。能力描述、路由和状态由同一注册表生成，最多 8 项，检查重复 ID、重复 GPIO、单 ADC 占用和数值约束。GPIO 号码不是 Dx 标签；GPIO0/2/15 有启动电平要求，GPIO1/3 留给串口。当前新外设只使用 D1/D2/D5/D6/A0。

设备名、资源名和属性显示名按 UTF-8 Unicode 字符计数，要求 1–64 字符、不能全为空白、不能含 C0/C1 控制字符。拒绝无效 UTF-8；完整配置验证成功后才初始化 GPIO。

二态属性为 boolean on；PWM 属性为 integer level，0–100%、step=1、1 kHz。电位器同一资源有两个只读属性：integer raw 与 number voltage（V），使用同一次缓存采样，每 250 ms 更新；安卓现有多属性能力协议即可显示，无需升级协议或改客户端。

电压按用户确认的 3.3 V 基准估算：`round(raw × 3.3 / 1024, 3)`，512 对应 1.650 V，1024 对应 3.300 V。raw 保留 SDK uint16 返回的原始计数（0–65535 是返回值容器范围，不是 ADC 的位数/精度）；超过标称 1024 时仍显示 raw，voltage 为 null/unavailable，避免伪造超量程电压。负数或超出 SDK 返回值类型范围才使两项均 unavailable。首次采样前两项同样为 null/unavailable。

旧版超过 1023 即显示不可用是软件判断，不是硬件过压报警；ADC 计数或饱和无法反推实际输入超过多少。3.3 V 是软件换算模型，非校准结果或硬件保护。PWM LED 每只单独串 1 kΩ 电阻；A0 接线说明仍区分板级分压与裸芯片限制。电机缺少驱动和额定参数，光敏器件按用户决定延后，均不注册。

安卓按清单给定的 0–100 范围发送百分数：50 表示 50%，不是 0.5。客户端统一使用 Double，因此线上可能是 `50.0`。固件 0.3.1 接受整数值的 JSON 数字（如 50、50.0、5e1），继续拒绝 50.5、字符串、布尔值、非有限数、越界和不符合步长的目标；不会通过截断把小数转成整数。0.3.0 的类型校验会误拒绝这类 `.0` 请求，需重新构建/烧录修复版，详见 [PWM 数值兼容验证](docs/testing/2026-10-16-pwm-json.md)。

## 开发约定

从 [AGENTS.md](AGENTS.md) 进入，按任务阅读 [.agents](.agents/README.md) 中的固件、协议、验证规则和开发流程。`.gitignore` 排除构建缓存、下载工具链和 Python 环境；`WiFiConfig.h` 明确保留在版本管理中。脱敏验收证据可提交，原始私密记录放在 `docs/testing/evidence/local/`。

## 验证

本机契约测试编译并执行同一 API/HTTP/驱动代码，使用 GPIO 替身覆盖幂等性、bootId、无效请求无副作用、长度边界、严格 JSON 和驱动失败。它不证明无线网络或真实引脚行为。

```powershell
# 首次通过 PlatformIO 安装 Windows 测试编译器：
pio pkg install --global --tool platformio/toolchain-gccmingw32@1.50100.0
./tools/test_native.ps1 -Compiler '你的g++.exe绝对路径'
# 烧录后连接板端热点，执行真实设备契约检查：
python tools/check_device.py --host http://192.168.4.1 --cycles 10
```

当前工作区可直接运行 `./tools/test_native.ps1`，它还会检查 STA/AP 的 mDNS 注册、断线、同 IP 重连、地址变化、失败重试、计时回绕及空 SSID，并在 `.tools/tests/` 导出实际 API 序列化结果。默认 STA 联调的 `--host` 必须换成串口或发现结果中的真实地址。

可选：用上位机固定提交的真实 Schema 检查这些输出，无需克隆仓库：

```powershell
python -m pip install --target .tools/python -r tools/requirements-test.txt
New-Item -ItemType Directory -Force .tools/upstream | Out-Null
Invoke-WebRequest 'https://raw.githubusercontent.com/zxu-source/Android_WIFI/320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b/protocol/capability-list.schema.json' -OutFile .tools/upstream/capability-list.schema.json
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python tests/check_capabilities.py --schema .tools/upstream/capability-list.schema.json
```

Schema 检查同时核对 device/state 清单版本、身份、属性集合、混合属性值/质量及写回报。最新结果见 [电压双属性验证](docs/testing/2026-10-16-pot-voltage.md)；历史结果保留在 [外设扩展验证](docs/testing/2026-10-16-peripherals.md)与[清单适配记录](docs/testing/2026-10-16-capability-adaptation.md)。用户要求每次修改完成后验证、提交、推送，具体流程已写入开发约定。

实物脚本会实际开关 LED：10 轮对应 20 次状态变化，额外检查重复写、类型、旧会话、JSON、媒体类型和超限请求，正常结束输出关闭。测试期间保持单客户端，人工观察 LED；脚本不自动烧录。未连接真实板前，不宣称实物或 Android 真机验收通过。
