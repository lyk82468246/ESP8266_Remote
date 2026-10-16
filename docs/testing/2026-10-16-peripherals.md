# 四 LED 与 A0 电位器扩展验证

日期：2026-10-16。固件：0.3.0。用户提供板卡文字为 LOLIN / wemos.cc / NEW NodeMCU V3；电机规格未知且没有驱动，光敏器件由用户决定延后。接线和当前范围见 [硬件说明](../hardware/peripherals.md)。上位机当前提交仍为 `320f9c00b35aa7c80edd5911832b7a2ccd5c1c3b`，本次只读取功能契约，没有克隆或修改安卓工程。

## 完成范围

- 原板载 LED 保留；新增 D1/GPIO5 红色开关 LED、D2/GPIO4 绿色开关 LED、D5/GPIO14 红色 PWM LED、D6/GPIO12 绿色 PWM LED，外接输出均高电平有效。
- PWM 是 0–100%、步长 1 的 integer level，1 kHz 软件波形；Core startWaveform/stopWaveform 成功后才确认目标。端点 0/100% 使用静态电平。
- 电位器通过唯一 A0 读取，只读 raw=0–1023；250 ms 主循环采样，HTTP 读取缓存。尚无有效采样或驱动返回无效范围时为 null/unavailable。采样相同值不递增 revision，值或可用性变化递增；全局 revision 统一归资源注册表管理。
- 6 项能力均来自同一注册配置。缺失/非法配置、引脚重复、重复 ID、重复 A0 和非法范围/步长在 GPIO 初始化前拒绝。电机、光敏不进入能力清单。
- 保留项目固定 SmartLife WiFi 配置及 mDNS/清单版本行为；本轮没有运行时热插拔，不改变 capabilitiesRevision=1 的语义。

## 已执行

| 检查 | 结果 | 范围 |
| --- | --- | --- |
| API/HTTP/驱动 C++ | 284 项断言通过 | 原协议回归、通道独立、严格 PWM 整数/范围/步长、同值幂等、0/25/100% 映射、波形创建/停止失败无成功回报、只读拒绝、ADC 缓存/采样节流/无效与恢复、全局 revision、空清单与非法注册 |
| 网络 C++ | 64 项断言通过 | STA 32、AP 27、空 SSID 5；已有 mDNS 与网络生命周期回归 |
| 上位机 Schema | 通过 | 使用固定上位机 Schema 校验实际序列化的六资源描述；检查混合 boolean/integer/null 值、quality、状态身份与清单版本 |
| STA 构建 | 通过 | RAM 45632 / 81920（55.7%）；Flash 321179 / 1044464（30.8%） |
| AP 构建 | 通过 | RAM 45600 / 81920（55.7%）；Flash 321235 / 1044464（30.8%） |

共 348 项 C++ 断言。构建基线不变：PlatformIO Core 6.1.18，espressif8266 4.2.1，Arduino Core 3.1.2，ArduinoJson 6.21.5。Core 自带 elf2bin.py 在 Python 3.12 下出现既有的正则转义 SyntaxWarning，构建成功；项目 C++ 未报告警告。

```powershell
./tools/test_native.ps1
./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python tests/check_capabilities.py --schema .tools/upstream/capability-list.schema.json
git diff --check
```

本机测试包含 GPIO、波形和 ADC 替身，没有触碰物理端口。Schema 检查使用实际 API 输出，未复制模拟服务器的五外设样例当作固件能力。

## 未执行与实物边界

未烧录；未连接真实 LED、电位器或电机；未验证亮度、PWM 波形精度/负载表现、A0 板载分压、真实采样值或 Android 真机显示。RAM 数值是静态构建容量，不替代网络/PWM 同时运行的堆与时序实测。

用户需要逐项按接线说明验证。A0 保守方案限制在约 0.77 V 以下，不以开发板标识推定可直接输入 3.3 V。ADC 不能可靠识别断线或把 raw 自动解释为电压/百分比；PWM 确认的是目标占空比而非实际亮度。电机没有驱动和额定参数，暂不分配 GPIO 或沿用灯光断联策略；光敏仅记录为后续任务。
