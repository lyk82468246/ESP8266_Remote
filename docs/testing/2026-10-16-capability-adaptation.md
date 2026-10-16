# Android_WIFI 功能清单适配验证

日期：2026-10-16。固件：0.2.0。上位机基线：[320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b](https://github.com/zxu-source/Android_WIFI/commit/320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b)。只读取协议、客户端相关代码、Schema 和验证记录，没有克隆上位机工程，也没有运行或修改 Android 源码。

以下为初次适配时的空 SSID 构建记录。此后用户要求将 WiFi 固定为项目配置，当前状态见末尾追加记录。

## 适配行为

- device/state 同时提供 `capabilitiesRevision=1`，身份与 bootId 一致；普通开关只递增状态 revision，同值写不递增。
- 当前清单来自编译时注册表，在本次启动内不可变。配置修改通过重新构建/重启生效，bootId 改变后清单版本可重新从 1 开始；未宣称支持运行时热插拔。
- 默认 STA 加入手机热点/路由器，SSID 为空时关闭 WiFi 并停用 API，输出保持关闭。保留显式 SoftAP 环境；不自动切换热点。真实凭据未填写，也未编入本次构建。
- mDNS 使用 `_wifi-control._tcp.local.`，实例名 `WiFi-Control-<deviceId>`，TXT 包含协议版本与设备 ID。断线清理，重连/地址变化后重新注册；失败每 5 秒重试，发现失败不影响手动 IP API。
- 设备、外设、属性显示名统一校验 1–64 个 Unicode 字符、非全空白、无 C0/C1 控制字符，拒绝无效 UTF-8。GPIO 初始化前完成配置验证。

## 已执行验证

| 检查 | 结果 | 范围 |
| --- | --- | --- |
| C++ 协议/HTTP/驱动/名称 | 207 项断言通过 | 保留原协议错误与幂等测试，新增清单版本与身份匹配、版本不随开关变化、UTF-8 字符计数、空白/控制字符/畸形编码边界 |
| STA 网络生命周期 | 32 项断言通过 | DHCP 前不注册、连接后注册与 update、同 IP 断线重连、IP 变化、begin/service/TXT 失败限频重试、计时回绕、日志不含测试密码 |
| AP 网络生命周期 | 27 项断言通过 | 显式模式和 SSID、注册内容、地址变化、失败重试、计时回绕 |
| 空 SSID | 5 项断言通过 | 网络关闭、拒绝启动、不注册 mDNS、有明确诊断 |
| 上位机 Schema | 通过 | 实际 API 序列化的 device 响应通过固定提交的 Schema；另校验 device/state/receipt 身份、清单版本、属性集合、值类型与响应长度 |
| 默认 STA 编译 `nodemcuv2` | 通过 | RAM 44992 / 81920（54.9%）；Flash 315535 / 1044464（30.2%）；无 SSID 配置，本产物会停用网络并提示配置 |
| 显式 AP 编译 `nodemcuv2_ap` | 通过 | RAM 44992 / 81920（54.9%）；Flash 317527 / 1044464（30.4%） |
| Python 工具 | 语法及入口检查通过 | `check_device.py` 显式要求 --host 并检查清单版本，未对真实设备运行 |

合计 271 项 C++ 断言。网络测试使用 WiFi/mDNS/时间替身，编译的是同一 `NetworkService.h`；本机测试忽略本地网络配置，使用测试专用凭据。固件构建则使用真实 Arduino Core 3.1.2 的 WiFi 与 mDNS 库。

命令：

```powershell
./tools/test_native.ps1
./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python tests/check_capabilities.py --schema .tools/upstream/capability-list.schema.json
python -m py_compile tools/check_device.py tests/check_capabilities.py
python tools/check_device.py --help
git diff --check
```

Schema 测试依赖及固定 URL 见 README；缓存与 API 导出文件位于忽略的 `.tools/`。以上容量不包括运行时网络堆增长；填入本地凭据后应重新构建再检查容量。初次完整构建出现 Arduino Core 自带 `elf2bin.py` 的 Python 3.12 正则转义 SyntaxWarning，构建成功；项目 C++ 未报告警告。

## 未执行

未烧录开发板；未验证真实 GPIO/LED、手机热点加入与 DHCP、STA 断线恢复、跨设备 mDNS 发现或 Android App 控制。未连接真实 WiFi，也未读取用户网络凭据。最终联调需填写本地配置、使用 2.4 GHz 热点，记录串口地址，在 App 选择对应路由模式并人工观察 LED。

## 固定项目 WiFi 配置（追加）

用户明确要求默认加入 SmartLife，并把所提供的网络参数固定保存在项目中。当前 `ESP8266_Remote/WiFiConfig.h` 是受版本管理的网络配置入口，默认 STA；不再加载 config.local.h，旧本地配置与样例已删除，相关 Git 忽略项已移除。README 和开发规则同步此约定。网络替身测试在编译时覆盖测试专用网络参数，保留空 SSID 的错误分支测试。

此次修改后 271 项本机断言通过，项目 WiFi 文件未被 Git 忽略，文档引用及 diff 格式检查通过。默认 STA 构建通过：RAM 45008 / 81920（54.9%），Flash 317479 / 1044464（30.4%）；显式 AP 构建通过：RAM 44992 / 81920（54.9%），Flash 317527 / 1044464（30.4%）。未烧录、未连接实际 WiFi，未执行真板或手机联调。
