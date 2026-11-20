# D7 GPIO 中断与 20 Hz 采样验证

固件升级为 0.9.0。本次只把 D7/GPIO13 微波输入改为 `CHANGE` GPIO 中断，并为 STA WiFi 增加 GotIP/Disconnected 事件回调；HTTP 和 mDNS 主循环处理保持不变。A0 电位器与红绿 PWM 联动采样周期从 250 ms 调整为 50 ms（20 Hz）。

实现边界：

- D7 ISR 仅记录电平和 `millis()` 到 4 槽有界队列；主循环取出事件、执行 20 ms 软件消抖、更新 `detected`/`offDelayMs`、revision 和板载 LED；队列溢出时在主循环重新读取 GPIO，不在 ISR 中做网络、JSON、Serial 或输出操作。
- STA 地址由 GotIP/Disconnected 回调设置待处理标志，主循环消费标志并完成 mDNS 注册/注销；SoftAP 没有对应 got-IP 事件，保留地址 sanity check。
- A0 仍在普通主循环上下文读取，未把 `analogRead()` 放入中断；标准 `WiFiClient` 收发轮询、mDNS.update() 和 `millis()` 超时逻辑未改。

验证结果：

- `./tools/test_native.ps1`：392 项协议/HTTP/驱动断言，以及 32/27/5 项 STA、AP、空 SSID 网络生命周期断言通过；网络替身改为发出 GotIP/Disconnected 事件，D7 测试通过中断替身触发边沿。
- `./tools/pio.ps1 run -e nodemcuv2 -e nodemcuv2_ap`：STA RAM 46512/81920（56.8%）、Flash 327423/1044464（31.3%）；AP RAM 46484/81920（56.7%）、Flash 326063/1044464（31.2%）。
- ELF 关键段：STA/AP `.text1` 均为 29025 字节；按 ESP8266 32 KiB flash cache 保留区计算，IRAM 仍低于 64 KiB 上限，但余量比 0.8.0 更紧，后续新增 ISR 前必须重新检查。
- `tests/check_capabilities.py` Schema 检查通过；`python -m py_compile tests/check_capabilities.py tools/check_device.py`、`python tools/check_device.py --help` 和 `git diff --check` 通过。
- 本轮实物烧录与真实微波/电位器联调在提交前尚未执行；烧录结果和串口启动记录将在上传后补记。
