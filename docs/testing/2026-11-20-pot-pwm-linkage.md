# 电位器与红绿 PWM 联动验证

> 历史记录：本页记录 0.8.0 的首次联动实现，当时采样周期为 250 ms。当前固件为 0.9.0，A0/PWM 联动周期为 50 ms（20 Hz）；当前构建与烧录证据见 [D7 GPIO 中断与 20 Hz 采样验证](2026-11-20-interrupt-sampling.md)。

本次将固件升级为 0.8.0。`led.red`/`pwm.red` 与 `led.green`/`pwm.green` 各自独立：对应开关灯关闭时，PWM 每 250 ms 跟随 A0 电位器；对应开关灯打开时，PWM 不再被电位器覆盖，由上位机 PUT 设置。

电位器 raw 到 PWM 的换算为 `round(raw × 100 / 1024)`，超出名义 10 位量程的诊断值饱和到 100%，不改变 raw/voltage 的既有报告规则。状态接口始终返回驱动确认的当前 PWM 值，因此联动时安卓轮询会覆盖其滑块显示。

契约测试覆盖：

- 红绿两路初始跟随同一电位器采样；
- 联动状态下上位机临时写入会在下一次采样被实际电位器值覆盖；
- 打开红色开关灯后红色 PWM 保持并接受上位机设置，绿色仍继续跟随；
- 红色切回联动后恢复电位器，绿色打开后保持上位机值；
- `/api/v1/state` 返回两个最终 PWM 值。

本机验证结果：

- `./tools/test_native.ps1`：392 项协议/HTTP/驱动断言，以及 32/27/5 项 STA、AP、空 SSID 网络生命周期断言通过；
- `./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap`：STA RAM 45920/81920（56.1%）、Flash 324315/1044464（31.1%）；AP RAM 45888/81920（56.0%）、Flash 324355/1044464（31.1%）；
- `tests/check_capabilities.py --schema .tools/upstream/capability-list-latest.schema.json`、Python 语法检查和 `git diff --check` 通过。

实物烧录记录：

- `./tools/pio.ps1 run -e nodemcuv2 --target upload --upload-port COM6` 成功；识别 USB-SERIAL CH340 / ESP8266EX，写入 328,464 字节，Hash 校验通过并自动复位；
- `COM6 / 115200` 串口看到 `API: http://192.168.39.220/api/v1/device` 和 `_wifi-control._tcp` mDNS 注册，启动后进入 SmartLife STA 流程。

尚未连接并观察真实 LED/A0，也没有 Android 无线联调证据；联动行为仍需在 NodeMCU 上实际转动电位器、切换两只布尔灯并观察安卓滑块。
