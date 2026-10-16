# 电位器原始计数与电压双属性

日期：2026-10-16。固件：0.4.0，capabilitiesRevision=2。用户明确要求以 3.3 V 为基准换算，同时显示原始值和电压，并授权以后每次完成修改、验证后自动提交推送。

## 协议与客户端

无需升级协议或修改安卓：现有 v1 每个资源允许 1–4 个属性，支持只读 number 和 V 单位。只给 sensor.pot 增加 voltage，不增加新资源或专用接口。raw 与 voltage 来自同一次缓存，不重复调用 ADC；二者分别有 quality。PUT 两项都返回 422 READ_ONLY。

重新检查的安卓提交仍是 `320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b`：[多属性规范](https://github.com/zxu-source/Android_WIFI/blob/320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b/docs/protocol/capability-list.md)、[只读属性显示代码](https://github.com/zxu-source/Android_WIFI/blob/320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b/app/src/main/java/edu/wifi/light/feature/control/ControlScreen.kt#L144-L151)。仅静态核对相关文件，没有克隆或运行安卓工程。

## 换算与端点语义

公式：`voltage = round(raw × 3.3 / 1024, 3)`，单位 V。配置位于 Config.h：kAdcFullScaleCount=1024、kAdcFullScaleVolts=3.3。这里使用用户指定的板端估算模型，没有声称实测校准或验证耐压。

| 原始计数 | raw 状态 | voltage 状态 |
| --- | --- | --- |
| 首次采样前 | null / unavailable | null / unavailable |
| 0 | 0 / good | 0 V / good |
| 512 | 512 / good | 1.650 V / good |
| 1023 | 1023 / good | 3.297 V / good |
| 1024 | 1024 / good | 3.300 V / good |
| 1025 | 1025 / good | null / unavailable，不作超量程外推 |
| 65535 | 原始返回值保留 | null / unavailable |
| 负数或 >65535 | null / unavailable | null / unavailable |

Arduino Core 3.1.2 的 analogRead(A0) 直接返回 SDK system_adc_read()；该 SDK 接口以 uint16 返回。65535 是容器上限，不是 ADC 分辨率。标称换算尺度参考 [Non-OS SDK API Guide 第 23 页](https://www.espressif.com/sites/default/files/2c-esp8266_non_os_sdk_api_guide_en_v1.5.4.pdf#page=23) 的 1/1024 V 单位及用户指定的板端 3.3 V 比例；它不能替代对板级分压的校准。

旧版软件用 raw>1023 判定 unavailable；该判断不是硬件过压报警。用户只看到了 unavailable，未取得实际原始值，因此本次不能断定当时是 1024、更大的计数、物理过压或其他校准/采样变化。新版本保留原始计数，让后续诊断有实际数值；不能从计数/饱和推断“只是轻微过压，所以安全”。

raw good 只表示原始返回值可读且符合其类型范围，voltage good 表示处于所配置的换算范围，不是电气安全标记。两项从一次采样派生，全局 revision 仍只增加一次。联调工具也已允许 ADC 在请求之间推动全局 revision，检查目标状态及版本单调性，不再要求不同 HTTP 请求之间计数完全不变。

## 已执行验证

- 345 项协议/HTTP/驱动断言通过；64 项网络生命周期断言通过，合计 409 项。
- 覆盖双属性能力、只读写入拒绝、首次空值、512 换算、1023/1024 边界、1025 保留 raw、SDK 容器边界、无效/恢复、质量和全局 revision。
- 上位机 Schema 与实际 API 输出对照通过；除初始状态外，还验证有效双属性和超标称范围状态。
- STA 构建通过：RAM 45728 / 81920；Flash 322075 / 1044464。
- AP 构建通过：RAM 45696 / 81920；Flash 322115 / 1044464。
- 未烧录，未用手机验证新版显示，也未测量实际电压或超压。需上传 0.4.0 后重新连接/更新能力清单。

```powershell
./tools/test_native.ps1
./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python tests/check_capabilities.py --schema .tools/upstream/capability-list.schema.json
python -m py_compile tools/check_device.py tests/check_capabilities.py
git diff --check
```

历史结果保留在之前的适配/PWM 记录，不将其当成 0.4.0 的实物证据。本次提交包含此前尚未提交的 0.2–0.3 系列源码、固定项目 WiFi 配置、硬件接线与测试文档，以及本次修改；不提交构建产物与工具链。
