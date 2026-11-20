# 微波感应输入验证（2026-11-20）

本次把用户提供的“微波感应开关”按只读二态传感器接入固件：`sensor.microwave.detected`，使用 D7/GPIO13 的 `INPUT_PULLUP`。默认假设模块外露两线是常开、无源继电器触点，触发后闭合到 GND；模块的定时、距离和光照旋钮仍由模块本身处理。

## 已执行

- `./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap`：STA/AP 构建通过；STA 使用 RAM 45,808/81,920（55.9%）、Flash 322,787/1,044,464（30.9%），AP 使用 RAM 45,792/81,920（55.9%）、Flash 322,811/1,044,464（30.9%）。
- `./tools/test_native.ps1`：361 项协议/HTTP/驱动断言，以及 32/27/5 项 STA、AP、空 SSID 网络生命周期断言通过；契约测试覆盖只读 boolean sensor 的清单描述、初始状态、闭合/释放状态变化、revision 递增和 PUT 返回 `READ_ONLY`。
- `PYTHONPATH=.tools/python; python tests/check_capabilities.py --schema .tools/upstream/capability-list-latest.schema.json`：使用上位机 Schema 校验新增 boolean sensor 的 device/state/receipt 输出。
- `./tools/pio.ps1 run -e nodemcuv2 --target upload --upload-port COM6`：检测到 USB-SERIAL CH340（COM6）和 ESP8266EX，写入后 Hash 校验通过并自动复位。
- 串口监视器 `COM6 / 115200`：看到 STA 启动、API 地址和 mDNS 注册日志，确认新固件已启动。
- `git diff --check`：通过。

## 未执行与边界

固件已经烧录并在真板启动，但未知模块尚未接到 ESP8266，也没有确认其两线是干接点、开漏输出还是带电压的负载端；尚无继电器触点状态、Android 真机控制或传感器联调证据。接线前必须断电，用万用表确认空闲开路、触发导通且不携带 5/12/24 V 或市电；否则要改用额定的光耦/数字隔离电路，不能直连 D7。
