# 微波感应输入验证（2026-11-20）

> 历史记录：本页记录 0.7.0 的首次接入和验证边界。当前固件为 0.9.0，D7/GPIO13 已改为 GPIO 中断并增加 20 ms 主循环消抖；当前烧录与启动证据见 [D7 GPIO 中断与 20 Hz 采样验证](2026-11-20-interrupt-sampling.md)。

本次把用户提供的“微波感应开关”接入固件 0.7.0：`sensor.microwave.detected` 使用 D7/GPIO13 的 `INPUT_PULLUP`，并新增只读 `offDelayMs` 毫秒统计。输入边沿自动控制 `led.main`；手机写入板载 LED 后在当前传感器状态周期内优先，下一次输入边沿恢复自动控制。默认假设模块外露两线是常开、无源继电器触点，触发后闭合到 GND；模块的定时、距离和光照旋钮仍由模块本身处理。

## 已执行

- `./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap`：STA/AP 构建通过；STA 使用 RAM 45,904/81,920（56.0%）、Flash 323,763/1,044,464（31.0%），AP 使用 RAM 45,888/81,920（56.0%）、Flash 323,811/1,044,464（31.0%）。
- `./tools/test_native.ps1`：375 项协议/HTTP/驱动断言，以及 32/27/5 项 STA、AP、空 SSID 网络生命周期断言通过；契约测试覆盖 boolean sensor 清单、`offDelayMs` 首次 unavailable 与完整周期值、自动点亮/熄灭、手机覆盖和只读 PUT 返回 `READ_ONLY`。
- `PYTHONPATH=.tools/python; python tests/check_capabilities.py --schema .tools/upstream/capability-list-latest.schema.json`：使用上位机 Schema 校验 boolean sensor、只读 integer `offDelayMs` 的 device/state/receipt 输出。
- `./tools/pio.ps1 run -e nodemcuv2 --target upload --upload-port COM6`：检测到 USB-SERIAL CH340（COM6）和 ESP8266EX，0.7.0 固件写入 327,920 字节，Hash 校验通过并自动复位。
- 串口监视器 `COM6 / 115200`：看到 0.7.0 启动并进入 STA 连接流程；未接微波触点，因此没有做传感器边沿和 LED 实物验收。
- `git diff --check`：通过。

## 未执行与边界

0.7.0 已烧录并在真板启动，但未知模块尚未接到 ESP8266，也没有确认其两线是干接点、开漏输出还是带电压的负载端；尚无继电器触点状态、Android 真机控制或传感器联调证据。接线前必须断电，用万用表确认空闲开路、触发导通且不携带 5/12/24 V 或市电；否则要改用额定的光耦/数字隔离电路，不能直连 D7。
